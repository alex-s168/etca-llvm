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
  // Without SAF, nothing is callee-saved (can't have function calls).
  if (ST.hasSAF()) {
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

  // Replace the frame index operand with FrameReg + offset.
  // For LOAD/STORE, the address must be in a register.
  // We handle this by keeping the frame index as-is if we can;
  // the MC layer will resolve it for direct LOAD/STORE with FI operands.
  //
  // For MOVZ with FI, replace FI with FrameReg + immediate.
  unsigned Opc = MI.getOpcode();
  if (Opc == LOAD16 || Opc == LOAD8 || Opc == LOAD32 || Opc == LOAD64 ||
      Opc == STORE16 || Opc == STORE8 || Opc == STORE32 || Opc == STORE64) {
    // LOAD/STORE can encode FrameIndex directly — the MC layer handles it.
    // Just replace with the frame register.
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
    return false;
  }

  // For MOVZ/MOVZI, we need to compute the offset.
  // Insert an ADD between the frame register and the offset.
  if (Opc == MOVZI16 || Opc == MOVZ16) {
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
    // The offset remains as an immediate which the instruction encodes.
    return false;
  }

  llvm_unreachable("Unexpected opcode in eliminateFrameIndex");
  return false;
}

const uint32_t *
ETCARegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const {
  // SAF ABI: R3(s0), R4(s1), R5(bp), R6(sp) are callee-saved.
  // The CSR_ETCA_RegMask is auto-generated from the CalleeSavedRegs definition
  // in ETCACallingConv.td.
  return getRegMasks()[0];
}

Register ETCARegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  // With SAF: use bp (r5).  Without SAF: use sp (r6) equivalent.
  if (ST.hasSAF())
    return R5;
  return R6;
}
