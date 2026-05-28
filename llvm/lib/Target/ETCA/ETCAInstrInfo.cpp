//===- ETCAInstrInfo.cpp - ETCA Instruction Information -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ETCAInstrInfo.h"
#include "ETCARegisterInfo.h"
#include "ETCASubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_INSTRINFO_ENUM
#define GET_REGINFO_ENUM
#define GET_INSTRINFO_CTOR_DTOR
#include "ETCAGenInstrInfo.inc"
#include "ETCAGenRegisterInfo.inc"

#define DEBUG_TYPE "etca-instr-info"

using namespace llvm;
using namespace ETCA;

ETCAInstrInfo::ETCAInstrInfo(ETCASubtarget &ST, const ETCARegisterInfo &RI)
    : ETCAGenInstrInfo(ST, RI, ETCA::ADJCALLSTACKDOWN, ETCA::ADJCALLSTACKUP),
      RegInfo(RI), ST(ST) {}

const TargetRegisterInfo &ETCAInstrInfo::getRegisterInfo() const {
  return RegInfo;
}

unsigned ETCAInstrInfo::getRegClassSize(const TargetRegisterClass &RC) const {
  if (&RC == &GPR8RegClass)
    return 1; // 8-bit
  if (&RC == &GPRRegClass)
    return 2; // 16-bit
  if (&RC == &GPR32RegClass)
    return 4; // 32-bit
  if (&RC == &GPR64RegClass)
    return 8; // 64-bit
  llvm_unreachable("Unknown register class in getRegClassSize");
}

bool ETCAInstrInfo::isMoveInstr(const TargetRegisterInfo &TRI,
                                const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  default:
    return false;
  case MOVZ16:
  case MOVZ32:
  case MOVZ64:
  case MOVS16:
  case MOVS32:
  case MOVS64:
    return true;
  }
}

Register ETCAInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
  unsigned Opc = MI.getOpcode();
  if (Opc == LOAD8 || Opc == LOAD16 || Opc == LOAD32 || Opc == LOAD64) {
    if (MI.getOperand(1).isFI()) {
      FrameIndex = MI.getOperand(1).getIndex();
      return MI.getOperand(0).getReg();
    }
  }
  return Register();
}

Register ETCAInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                           int &FrameIndex) const {
  unsigned Opc = MI.getOpcode();
  if (Opc == STORE8 || Opc == STORE16 || Opc == STORE32 || Opc == STORE64) {
    if (MI.getOperand(1).isFI()) {
      FrameIndex = MI.getOperand(1).getIndex();
      return MI.getOperand(0).getReg();
    }
  }
  return Register();
}

void ETCAInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                const DebugLoc &DL, Register DestReg,
                                Register SrcReg, bool KillSrc,
                                bool RenamableDest, bool RenamableSrc) const {
  // If DestReg == SrcReg or they share the same underlying physical
  // storage (e.g., D0 has sub_16 → R0), the copy is a no-op.
  // REX extended registers (r8-r15, d8-d15) have coincident encoding
  // numbers but are DIFFERENT physical registers, so regsOverlap
  // correctly returns false for those.
  const TargetRegisterInfo &TRI = getRegisterInfo();
  if (DestReg == SrcReg || TRI.regsOverlap(DestReg, SrcReg))
    return;

  // Determine the appropriate MOVZ based on the register class.
  unsigned RegWidth = ST.getRegWidth();
  unsigned Opc;
  switch (RegWidth) {
  case 64:
    Opc = MOVZ64;
    break;
  case 32:
    Opc = MOVZ32;
    break;
  default:
    Opc = MOVZ16;
    break;
  }

  // RR format (non-tied): [dst, src]
  BuildMI(MBB, I, DL, get(Opc), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

void ETCAInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  MachineFunction &MF = *MBB.getParent();
  unsigned Size = getRegClassSize(*RC);
  Align Alignment(Size);

  unsigned StoreOpc;
  switch (Size) {
  case 8:
    StoreOpc = STORE64;
    break;
  case 4:
    StoreOpc = STORE32;
    break;
  case 2:
    StoreOpc = STORE16;
    break;
  case 1:
    StoreOpc = STORE8;
    break;
  default:
    llvm_unreachable("Unhandled register class size in storeRegToStackSlot");
  }
  DebugLoc DL = (I != MBB.end()) ? I->getDebugLoc() : DebugLoc();
  auto MIB = BuildMI(MBB, I, DL, get(StoreOpc))
                 .addReg(SrcReg, getKillRegState(isKill))
                 .addFrameIndex(FrameIndex)
                 .addMemOperand(MF.getMachineMemOperand(
                     MachinePointerInfo::getFixedStack(MF, FrameIndex),
                     MachineMemOperand::MOStore, Size, Alignment));
  if (Flags != MachineInstr::NoFlags)
    MIB.setMIFlags(Flags);
}

void ETCAInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         Register DestReg, int FrameIndex,
                                         const TargetRegisterClass *RC,
                                         Register VReg, unsigned SubReg,
                                         MachineInstr::MIFlag Flags) const {
  MachineFunction &MF = *MBB.getParent();
  unsigned Size = getRegClassSize(*RC);
  Align Alignment(Size);

  unsigned LoadOpc;
  switch (Size) {
  case 8:
    LoadOpc = LOAD64;
    break;
  case 4:
    LoadOpc = LOAD32;
    break;
  case 2:
    LoadOpc = LOAD16;
    break;
  case 1:
    LoadOpc = LOAD8;
    break;
  default:
    llvm_unreachable("Unhandled register class size in loadRegFromStackSlot");
  }
  DebugLoc DL = (I != MBB.end()) ? I->getDebugLoc() : DebugLoc();
  auto MIB = BuildMI(MBB, I, DL, get(LoadOpc), DestReg)
                 .addFrameIndex(FrameIndex)
                 .addMemOperand(MF.getMachineMemOperand(
                     MachinePointerInfo::getFixedStack(MF, FrameIndex),
                     MachineMemOperand::MOLoad, Size, Alignment));
  if (Flags != MachineInstr::NoFlags)
    MIB.setMIFlags(Flags);
}

bool ETCAInstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const TargetInstrInfo &TII = *this;
  DebugLoc DL = MI.getDebugLoc();

  switch (MI.getOpcode()) {
  default:
    return false;

    //===----------------------------------------------------------------===//
    // RET_Pseudo -> jmpr r7 (when SAF is available)
    // Without SAF, RET_Pseudo should not be generated.
    //===----------------------------------------------------------------===//

  case ETCA::RET_Pseudo: {
    assert(ST.hasSAF() && "RET_Pseudo requires SAF extension");
    auto Jmp = BuildMI(MBB, MI, DL, TII.get(ETCA::JMPR)).addReg(R7);
    // Copy implicit operands (e.g., return value register) from the pseudo.
    // Start at i = 0 since RET_Pseudo has 0 explicit operands and the first
    // operand (index 0) is the first implicit use (the return value register).
    for (unsigned i = 0, e = MI.getNumOperands(); i < e; ++i)
      if (MI.getOperand(i).isReg() && MI.getOperand(i).isImplicit())
        Jmp.add(MI.getOperand(i));
    MI.eraseFromParent();
    return true;
  }

    //===----------------------------------------------------------------===//
    // CALL_Pseudo -> CALL (short form, relaxed by MC layer when out of
    // range).  The MC assembler's relaxInstruction expands to MOV_*+CALLR
    // if the 12-bit displacement doesn't reach the target.
    //===----------------------------------------------------------------===//

  case ETCA::CALL_Pseudo: {
    assert(ST.hasSAF() && "CALL_Pseudo requires SAF extension");
    // CALL_Pseudo has operands: <callee> [implicit ops...]
    MachineInstr *NewCall = BuildMI(MBB, MI, DL, TII.get(ETCA::CALL));
    for (unsigned i = 0, e = MI.getNumOperands(); i < e; ++i) {
      const MachineOperand &MO = MI.getOperand(i);
      if (MO.isGlobal() || MO.isSymbol() || MO.isMBB() || MO.isReg() ||
          MO.isImm()) {
        NewCall->addOperand(MF, MO);
      }
    }
    MI.eraseFromParent();
    return true;
  }

    //===----------------------------------------------------------------===//
    // LONG_BR_Pseudo -> MOVZI + JMPR (long unconditional branch)
    // Uses R7 (the dedicated scratch register) to hold the target address.
    //===----------------------------------------------------------------===//

  // SELECT_Pseudo is expanded by the ETCASelectExpand pass.
  case ETCA::SELECT_Pseudo: {
    return false;
  }

  // ICMP_Pseudo is expanded during instruction selection (by G_BRCOND and
  // G_SELECT handlers) or during ETCASelectExpand (when the ICMP result
  // is used by other instructions).  If we get here, it's a leftover that
  // should have been handled — just leave it for the machine verifier.
  case ETCA::ICMP_Pseudo: {
    return false;
  }
  }
}

std::optional<DestSourcePair>
ETCAInstrInfo::isCopyInstrImpl(const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  default:
    break;
  case ETCA::MOVZ16:
  case ETCA::MOVZ32:
  case ETCA::MOVZ64:
  case ETCA::MOVS16:
  case ETCA::MOVS32:
  case ETCA::MOVS64:
    // Only recognize as copy when BOTH operands are registers.
    // MOVZ/MOVS with FrameIndex/GlobalAddress operands are NOT copies.
    if (MI.getOperand(0).isReg() && MI.getOperand(1).isReg())
      return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
    break;
  }
  return std::nullopt;
}
