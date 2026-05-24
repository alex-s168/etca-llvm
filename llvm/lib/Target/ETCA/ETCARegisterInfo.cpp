//===- ETCARegisterInfo.cpp - ETCA Register Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ETCARegisterInfo.h"
#include "ETCAInstrInfo.h"
#include "ETCASubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"

#define GET_INSTRINFO_ENUM
#define GET_REGINFO_ENUM
#define GET_REGINFO_TARGET_DESC
#include "ETCAGenInstrInfo.inc"
#include "ETCAGenRegisterInfo.inc"

#define DEBUG_TYPE "etca-register-info"

using namespace llvm;
using namespace ETCA;

ETCARegisterInfo::ETCARegisterInfo(const ETCASubtarget &ST)
    : ETCAGenRegisterInfo(R0), ST(ST) {}

const TargetRegisterClass *
ETCARegisterInfo::getPointerRegClass(unsigned Kind) const {
  // Return the register class matching the pointer size.
  switch (ST.getPtrSize()) {
  case 32:
    return &GPR32RegClass;
  case 64:
    return &GPR64RegClass;
  default:
    return &GPRRegClass; // 16-bit
  }
}

const MCPhysReg *
ETCARegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  // SAF ABI: r3(s0), r4(s1), r5(bp), r6(sp) are callee-saved.
  // With REX extension, r13(s2), r14(s3), r15(s4) are also callee-saved.
  // Without SAF, nothing is callee-saved (can't have function calls).
  if (ST.hasSAF()) {
    if (ST.hasREX()) {
      static const MCPhysReg CalleeSavedRegs_REX[] = {
          ETCA::R3,  ETCA::R4,  ETCA::R5,  ETCA::R6,
          ETCA::R13, ETCA::R14, ETCA::R15, 0};
      return CalleeSavedRegs_REX;
    }
    static const MCPhysReg CalleeSavedRegs[] = {ETCA::R3, ETCA::R4, ETCA::R5,
                                                ETCA::R6, 0};
    return CalleeSavedRegs;
  }
  static const MCPhysReg NoCalleeSaved[] = {0};
  return NoCalleeSaved;
}

BitVector ETCARegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  // R0 is a regular argument/return register.  It is NOT reserved — the
  // register allocator can allocate vregs to R0.
  //
  // R7 (ln) IS reserved.  It serves as a dedicated scratch register for
  // frame index elimination (see eliminateFrameIndex STORE path).  Since
  // ETCa LOAD/STORE instructions have no immediate offset field, address
  // computation requires a register.  Using a dedicated scratch avoids
  // the recursive spill problem where spilling to find a scratch register
  // creates new frame indices needing resolution.  R7 is also the link
  // register (return address), but CALL_Pseudo's Defs=[R7] and JMPR's use
  // of R7 work correctly with a reserved register.
  //
  // With SAF: reserve sp (r6), bp (r5), and ln (r7) as frame registers.
  // r7 is reserved as a dedicated scratch register for frame index
  // elimination (see eliminateFrameIndex STORE path).  Since ETCa
  // LOAD/STORE instructions have no immediate offset field, address
  // computation requires a register.  Using a dedicated scratch avoids
  // the recursive spill problem.
  // With SAF: reserve bp (r5), sp (r6), and ln (r7) as frame/scratch
  // registers.  Reserve ALL aliases (d5, q5, etc.) so that wider
  // register classes (GPR32, GPR64) correctly exclude them.
  if (ST.hasSAF()) {
    for (MCRegAliasIterator AI(ETCA::R5, this, true); AI.isValid(); ++AI)
      Reserved.set(*AI); // bp
    for (MCRegAliasIterator AI(ETCA::R6, this, true); AI.isValid(); ++AI)
      Reserved.set(*AI); // sp
    for (MCRegAliasIterator AI(ETCA::R7, this, true); AI.isValid(); ++AI)
      Reserved.set(*AI); // ln (dedicated scratch)
  }
  return Reserved;
}

bool ETCARegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                           int SPAdj, unsigned FIOperandNum,
                                           RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  Register FrameReg;
  StackOffset Offset = TFI->getFrameIndexReference(MF, FrameIndex, FrameReg);

  // Determine MOV/ADDI opcodes and register class for the address width.
  unsigned Opc = MI.getOpcode();
  unsigned MovOpc, AddiOpc;
  const TargetRegisterClass *RC = nullptr;
  bool IsLoad = false;

  auto setWidth = [&](unsigned Mov, unsigned Addi,
                      const TargetRegisterClass *RegClass) {
    MovOpc = Mov;
    AddiOpc = Addi;
    RC = RegClass;
  };

  switch (Opc) {
  case LOAD8:
    IsLoad = true;
    [[fallthrough]];
  case STORE8:
    setWidth(MOVZ8, ADDI8, &GPRRegClass);
    break;
  case LOAD16:
    IsLoad = true;
    [[fallthrough]];
  case STORE16:
    setWidth(MOVZ16, ADDI16, &GPRRegClass);
    break;
  case LOAD32:
    IsLoad = true;
    [[fallthrough]];
  case STORE32:
    setWidth(MOVZ32, ADDI32, &GPR32RegClass);
    break;
  case LOAD64:
    IsLoad = true;
    [[fallthrough]];
  case STORE64:
    setWidth(MOVZ64, ADDI64, &GPR64RegClass);
    break;
  default:
    // Not a LOAD/STORE — only MOVZ/MOVZI handled below.
    break;
  }

  if (RC) {
    int64_t Off = Offset.getFixed();

    if (Off == 0) {
      // Zero offset: just use the frame register directly.
      MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*isDef=*/false);
      return false;
    }

    // Non-zero offset: compute FrameReg + Offset into a scratch register,
    // then use that register for the LOAD/STORE.
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    DebugLoc DL = MI.getDebugLoc();

    if (IsLoad) {
      // Reuse the LOAD destination register as address-computation scratch.
      // This is safe because:
      //   1. The dest register is dead before this instruction (it's about
      //      to be overwritten), so MOVZ/ADDI into it cannot clobber any
      //      live value.
      //   2. FrameReg is always one of the reserved frame registers (R5=bp
      //      or R6=sp), which can never be allocated as the LOAD destination.
      //   3. The hardware reads the address operand before writing the
      //      result register — so reading ScratchReg for the address is
      //      fine even though the write to the same register follows.
      //
      // Generated sequence:
      //   MOVZ  dst, FrameReg       ; address = FrameReg
      //   ADDI  dst, dst, offset    ; address += offset
      //   LOAD  dst, (dst)          ; dst = *address
      Register ScratchReg = MI.getOperand(0).getReg();

      BuildMI(MBB, II, DL, TII.get(MovOpc), ScratchReg).addReg(FrameReg);

      int64_t Remaining = Off;
      while (Remaining != 0) {
        int64_t Step = std::clamp<int64_t>(Remaining, -16, 15);
        BuildMI(MBB, II, DL, TII.get(AddiOpc), ScratchReg)
            .addReg(ScratchReg)
            .addImm(Step);
        Remaining -= Step;
      }

      MI.getOperand(FIOperandNum).ChangeToRegister(ScratchReg, /*isDef=*/false);
      return false;
    }

    // For STORE, use the dedicated scratch register R7/D7/Q7 (reserved
    // in getReservedRegs) to compute the address, then use that register
    // for the STORE.  Using a dedicated scratch avoids calling the
    // register scavenger here, which would create a circular dependency:
    // the scavenger might spill a register via storeRegToStackSlot (which
    // has its own frame index), and eliminating that frame index would
    // need another scratch register — ad infinitum.
    //
    // WARNING: We must NOT modify FrameReg in place (e.g. ADDI FrameReg, +
    // ADDI FrameReg, -), because an interrupt between the modification
    // and the restore would leave the frame register corrupted.
    //
    // Map R7 to the right register class.
    Register ScratchReg = RC == &GPR64RegClass   ? ETCA::Q7
                          : RC == &GPR32RegClass ? ETCA::D7
                                                 : ETCA::R7;

    BuildMI(MBB, II, DL, TII.get(MovOpc), ScratchReg).addReg(FrameReg);

    int64_t Remaining = Off;
    while (Remaining != 0) {
      int64_t Step = std::clamp<int64_t>(Remaining, -16, 15);
      BuildMI(MBB, II, DL, TII.get(AddiOpc), ScratchReg)
          .addReg(ScratchReg)
          .addImm(Step);
      Remaining -= Step;
    }

    MI.getOperand(FIOperandNum).ChangeToRegister(ScratchReg, /*isDef=*/false);
    return false;
  }

  // For MOVZ/MOVS with FrameIndex (from G_FRAME_INDEX selection):
  // replace FI with FrameReg, then add the offset via ADDI.
  switch (Opc) {
  case MOVZ8:
  case MOVS8:
    AddiOpc = ADDI8;
    break;
  case MOVZ16:
  case MOVS16:
    AddiOpc = ADDI16;
    break;
  case MOVZ32:
  case MOVS32:
    AddiOpc = ADDI32;
    break;
  case MOVZ64:
  case MOVS64:
    AddiOpc = ADDI64;
    break;
  default:
    AddiOpc = 0;
    break;
  }

  if (AddiOpc) {
    int64_t Off = Offset.getFixed();

    if (Off == 0) {
      // Zero offset: MOVZ dst, FrameReg is sufficient.
      MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
      return false;
    }

    // Non-zero offset: MOVZ dst, FrameReg; ADDI dst, dst, offset
    Register Dst = MI.getOperand(0).getReg();
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);

    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    DebugLoc DL = MI.getDebugLoc();

    // Insert ADDI after the MOVZ (advance iterator past original MI)
    MachineBasicBlock::iterator NextII = std::next(II);
    int64_t Remaining = Off;
    while (Remaining != 0) {
      int64_t Step = std::clamp<int64_t>(Remaining, -16, 15);
      BuildMI(MBB, NextII, DL, TII.get(AddiOpc), Dst).addReg(Dst).addImm(Step);
      Remaining -= Step;
    }
    return false;
  }

  // MOVZI with FrameIndex (G_GLOBAL_VALUE path — handled by fixup/relocation)
  if (Opc == MOVZI16 || Opc == MOVZI32 || Opc == MOVZI64 || Opc == MOVZI8 ||
      Opc == MOVSI16 || Opc == MOVSI32 || Opc == MOVSI64 || Opc == MOVSI8) {
    // For MOVZI, the immediate will be resolved via a fixup/relocation.
    // Just replace the FrameIndex with an immediate 0 placeholder.
    // The actual resolution happens in the AsmBackend.
    MI.getOperand(FIOperandNum).ChangeToImmediate(0);
    return false;
  }

  llvm_unreachable("Unexpected opcode in eliminateFrameIndex");
  return false;
}

bool ETCARegisterInfo::requiresRegisterScavenging(
    const MachineFunction &MF) const {
  // The register scavenger is not needed because ETCa's
  // eliminateFrameIndex uses the dedicated scratch register R7/D7/Q7
  // (reserved in getReservedRegs) for STORE address computation,
  // instead of scavenging.  This avoids recursive spill-while-spilling.
  return false;
}

const uint32_t *
ETCARegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const {
  // SAF ABI: R3(s0), R4(s1), R5(bp), R6(sp) are callee-saved.
  // With REX: R13(s2), R14(s3), R15(s4) are also callee-saved.
  // The CSR mask is auto-generated from CalleeSavedRegs definitions
  // in ETCACallingConv.td.  Select the right mask based on word size
  // and REX availability, and the calling convention.
  //
  // getRegMasks() returns masks in the order they appear in the .td file:
  //   [0] = CSR_ETCA_RegMask (16-bit)
  //   [1] = CSR_ETCA_GPR32_RegMask (32-bit)
  //   [2] = CSR_ETCA_GPR64_RegMask (64-bit)
  //   [3] = CSR_ETCA_REX_RegMask (16-bit + REX)
  //   [4] = CSR_ETCA_REX_GPR32_RegMask (32-bit + REX)
  //   [5] = CSR_ETCA_REX_GPR64_RegMask (64-bit + REX)
  //   [6] = CSR_ETCA_PreserveMost_RegMask (16-bit)
  //   [7] = CSR_ETCA_PreserveMost_GPR32_RegMask (32-bit)
  //   [8] = CSR_ETCA_PreserveMost_GPR64_RegMask (64-bit)
  //   [9] = CSR_ETCA_PreserveMost_REX_RegMask (16-bit + REX)
  //  [10] = CSR_ETCA_PreserveMost_REX_GPR32_RegMask (32-bit + REX)
  //  [11] = CSR_ETCA_PreserveMost_REX_GPR64_RegMask (64-bit + REX)
  //  [12] = CSR_ETCA_PreserveAll_RegMask (16-bit)
  //  [13] = CSR_ETCA_PreserveAll_GPR32_RegMask (32-bit)
  //  [14] = CSR_ETCA_PreserveAll_GPR64_RegMask (64-bit)
  //  [15] = CSR_ETCA_PreserveAll_REX_RegMask (16-bit + REX)
  //  [16] = CSR_ETCA_PreserveAll_REX_GPR32_RegMask (32-bit + REX)
  //  [17] = CSR_ETCA_PreserveAll_REX_GPR64_RegMask (64-bit + REX)
  //  [18] = CSR_NoRegs_RegMask (empty — GHC)
  //
  // Select the mask group based on CC, then index by width/REX.

  // GHC: no registers preserved.
  if (CC == CallingConv::GHC)
    return getRegMasks()[18];

  // PreserveMost / Cold: preserve all registers except R0 (return/arg).
  //   CSR_ETCA_PreserveMost_* — indices 6-11 (non-REX: 6-8, REX: 9-11)
  if (CC == CallingConv::PreserveMost || CC == CallingConv::Cold) {
    if (ST.hasREX()) {
      if (ST.getWordSize() >= 64)
        return getRegMasks()[11];
      if (ST.getWordSize() >= 32)
        return getRegMasks()[10];
      return getRegMasks()[9];
    }
    if (ST.getWordSize() >= 64)
      return getRegMasks()[8];
    if (ST.getWordSize() >= 32)
      return getRegMasks()[7];
    return getRegMasks()[6];
  }

  // PreserveAll: preserve all registers including R0.
  //   CSR_ETCA_PreserveAll_* — indices 12-17
  if (CC == CallingConv::PreserveAll) {
    if (ST.hasREX()) {
      if (ST.getWordSize() >= 64)
        return getRegMasks()[17];
      if (ST.getWordSize() >= 32)
        return getRegMasks()[16];
      return getRegMasks()[15];
    }
    if (ST.getWordSize() >= 64)
      return getRegMasks()[14];
    if (ST.getWordSize() >= 32)
      return getRegMasks()[13];
    return getRegMasks()[12];
  }

  // Default (CallingConv::C): standard SAF ABI callee-saved set.
  //   CSR_ETCA_*_RegMask — indices 0-5
  if (ST.hasREX()) {
    if (ST.getWordSize() >= 64)
      return getRegMasks()[5];
    if (ST.getWordSize() >= 32)
      return getRegMasks()[4];
    return getRegMasks()[3];
  }
  if (ST.getWordSize() >= 64)
    return getRegMasks()[2];
  if (ST.getWordSize() >= 32)
    return getRegMasks()[1];
  return getRegMasks()[0];
}

Register ETCARegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  // With SAF: use bp (r5).  Without SAF: use sp (r6) equivalent.
  if (ST.hasSAF())
    return R5;
  return R6;
}
