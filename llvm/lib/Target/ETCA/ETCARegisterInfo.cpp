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
  // register allocator can allocate vregs to R0.  The calling convention
  // and CALL_Pseudo's implicit operands handle the liveness correctly.
  //
  // R7 (ln) is the link register — stores the return address on CALL and is
  // used by JMPR (return).  It is NOT reserved either.  The register
  // allocator handles the liveness of R7 across CALL_Pseudo via the
  // implicit-def on CALL_Pseudo and the use in JMPR.
  //
  // With SAF: reserve sp (r6) and bp (r5) as frame registers.
  if (ST.hasSAF()) {
    Reserved.set(R5); // bp
    Reserved.set(R6); // sp
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
      // LOAD destination register is dead before the instruction (it's
      // about to be overwritten), so reuse it as the scratch register.
      //   MOVZ scratch, FrameReg
      //   ADDI scratch, scratch, offset
      //   LOAD dst, scratch
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

    // For STORE, use a scratch register from the register scavenger to
    // compute the address, then use that register for the STORE.
    //   MOVZ scratch, FrameReg
    //   ADDI scratch, scratch, offset  (chained for large offsets)
    //   STORE val, scratch
    Register ScratchReg;
    if (RS) {
      ScratchReg = RS->scavengeRegisterBackwards(*RC, II, false, SPAdj);
    } else {
      // Fallback (rare): temporarily adjust the frame register in place,
      // use it as the address, then restore.
      SmallVector<int64_t, 4> Steps;
      int64_t Remaining = Off;
      while (Remaining != 0) {
        int64_t Step = std::clamp<int64_t>(Remaining, -16, 15);
        Steps.push_back(Step);
        BuildMI(MBB, II, DL, TII.get(AddiOpc), FrameReg)
            .addReg(FrameReg)
            .addImm(Step);
        Remaining -= Step;
      }
      MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*isDef=*/false);
      for (int64_t Step : llvm::reverse(Steps)) {
        BuildMI(MBB, std::next(II), DL, TII.get(AddiOpc), FrameReg)
            .addReg(FrameReg)
            .addImm(-Step);
      }
      return false;
    }

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
  // We need the register scavenger during frame index elimination to
  // provide a scratch register for addressing spills (STORE frame
  // operands), so that the frame pointer (r5) does not need to be
  // temporarily modified for each spill slot.
  return true;
}

const uint32_t *
ETCARegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const {
  // SAF ABI: R3(s0), R4(s1), R5(bp), R6(sp) are callee-saved.
  // With REX: R13(s2), R14(s3), R15(s4) are also callee-saved.
  // The CSR mask is auto-generated from CalleeSavedRegs definitions
  // in ETCACallingConv.td.  Select the right mask based on word size
  // and REX availability.
  // getRegMasks() returns masks in the order they appear in the .td file:
  //   [0] = CSR_ETCA_RegMask (16-bit)
  //   [1] = CSR_ETCA_GPR32_RegMask (32-bit)
  //   [2] = CSR_ETCA_GPR64_RegMask (64-bit)
  //   [3] = CSR_ETCA_REX_RegMask (16-bit + REX)
  //   [4] = CSR_ETCA_REX_GPR32_RegMask (32-bit + REX)
  //   [5] = CSR_ETCA_REX_GPR64_RegMask (64-bit + REX)
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
