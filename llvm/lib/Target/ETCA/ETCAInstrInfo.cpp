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
    : ETCAGenInstrInfo(ST, RI), RegInfo(RI), ST(ST) {}

const TargetRegisterInfo &ETCAInstrInfo::getRegisterInfo() const {
  return RegInfo;
}

unsigned ETCAInstrInfo::getRegClassSize(const TargetRegisterClass &RC) const {
  if (&RC == &GPRRegClass)
    return 2; // 16-bit
  if (&RC == &GPR32RegClass)
    return 4; // 32-bit
  if (&RC == &GPR64RegClass)
    return 8; // 64-bit
  return 2;   // default
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
  // If both registers map to the same underlying ETCa register number
  // (e.g., R0 and D0 both encode as register 0), the copy is a no-op.
  // MOVZ16/MOVS16/MOVZ32/MOVS32 all use 3-bit register fields in the
  // encoding, and R0, D0, Q0 all share the same encoding (0).
  if (DestReg != SrcReg) {
    const TargetRegisterInfo &TRI = getRegisterInfo();
    unsigned DestEnc = TRI.getEncodingValue(DestReg);
    unsigned SrcEnc = TRI.getEncodingValue(SrcReg);
    if ((DestEnc & 0x7) == (SrcEnc & 0x7))
      return; // Same physical register — copy is a no-op.
  }

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
  default:
    StoreOpc = STORE16;
    break;
  }
  auto MIB = BuildMI(MBB, I, I->getDebugLoc(), get(StoreOpc))
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
  default:
    LoadOpc = LOAD16;
    break;
  }
  auto MIB = BuildMI(MBB, I, I->getDebugLoc(), get(LoadOpc), DestReg)
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
    BuildMI(MBB, MI, DL, TII.get(ETCA::JMPR)).addReg(R7);
    MI.eraseFromParent();
    return true;
  }

    //===----------------------------------------------------------------===//
    // CALL_Pseudo -> CALL (SAF 12-bit PC-relative call)
    //===----------------------------------------------------------------===//

  case ETCA::CALL_Pseudo: {
    assert(ST.hasSAF() && "CALL_Pseudo requires SAF extension");
    // CALL_Pseudo has operands: <callee> [reg, reg...] [def reg]
    // The first operand after the opcode is the callee (global/external sym).
    MachineInstr *NewCall = BuildMI(MBB, MI, DL, TII.get(ETCA::CALL));

    // Copy operands from the pseudo
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

  // SELECT_Pseudo is expanded by the ETCASelectExpand pass.
  case ETCA::SELECT_Pseudo: {
    return false;
  }
  }
}
