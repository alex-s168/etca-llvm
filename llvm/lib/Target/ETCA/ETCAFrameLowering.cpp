//===- ETCAFrameLowering.cpp - ETCA Frame Lowering -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ETCA frame lowering.
//
// The stack grows down.  Stack alignment is register-width bytes.
// With SAF extension, r6 is the stack pointer (sp) and r5 is the base
// pointer (bp).  The frame is set up with:
//   push r5          ; save old bp
//   mov r5, r6       ; bp = sp
//   sub r6, N        ; allocate N bytes for locals
// and torn down with:
//   mov r6, r5       ; sp = bp
//   pop r5           ; restore old bp
//   jmpr r7 / ret    ; return (via SAF jmpr if SAF)
//
// Without SAF, there is no stack frame — functions can't call each other.
//===----------------------------------------------------------------------===//

#include "ETCAFrameLowering.h"
#include "ETCA.h"
#include "ETCASubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"

// Bring in the instruction and register enum constants.
#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"
#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"

#define DEBUG_TYPE "etca-frame-lowering"

using namespace llvm;
using namespace ETCA;

bool ETCAFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  // With SAF, we use r5 (bp) as the frame pointer.
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  return ST.hasSAF();
}

void ETCAFrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  if (!ST.hasSAF())
    return;

  MachineBasicBlock::iterator MBBI = MBB.begin();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetInstrInfo &TII = *ST.getInstrInfo();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  // SAF prologue:
  //   push r5          ; save old base pointer
  //   mov r5, r6       ; bp = sp
  //   sub r6, N        ; allocate N bytes for locals (if N > 0)

  // 1. push r5
  BuildMI(MBB, MBBI, DL, TII.get(PUSH)).addReg(R5);

  // 2. mov r5, r6 (copy sp to bp) — RR format (non-tied): [dst, src]
  BuildMI(MBB, MBBI, DL, TII.get(MOVZ16), R5).addReg(R6);

  // 3. Allocate stack for locals
  int StackSize = MFI.getStackSize();
  if (StackSize > 0) {
    // sub r6, StackSize — RI format: [dst, src1(tied), imm]
    BuildMI(MBB, MBBI, DL, TII.get(SUBI16), R6)
        .addReg(R6)
        .addImm(StackSize);
  }
}

void ETCAFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  if (!ST.hasSAF())
    return;

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  const TargetInstrInfo &TII = *ST.getInstrInfo();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  // SAF epilogue (insert before the return instruction):
  //   mov r6, r5       ; sp = bp
  //   pop r5           ; restore old bp

  // 1. mov r6, r5 — RR format (non-tied): [dst, src]
  BuildMI(MBB, MBBI, DL, TII.get(MOVZ16), R6).addReg(R5);

  // 2. pop r5
  BuildMI(MBB, MBBI, DL, TII.get(POP), R5);
}

StackOffset
ETCAFrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                          Register &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  const TargetRegisterInfo &TRI = *ST.getRegisterInfo();

  // FrameReg is bp (r5) with SAF, sp (r6) without.
  FrameReg = TRI.getFrameRegister(MF);

  // With SAF:
  //   bp = sp (after prologue)
  //   Stack frame layout (grows down):
  //     [old bp]      <- bp + 0
  //     [locals...]   <- bp - local_offset
  //
  //   FrameIndex offset: negative offset from bp for locals.
  //
  // The offset from bp to the local:
  //   offset = -(MFI.getObjectOffset(FI) + adjustment_for_saved_regs)
  //
  // Since we push r5 (2 bytes) and set bp=sp, the actual frame offset
  // is: bp - object_offset - 2 (for pushed bp)
  //
  // With SAF prologue, bp = old_sp, and sp = bp - StackSize.
  // Objects are allocated at SP + object_offset.
  // So relative to BP: bp - StackSize + object_offset = bp - (StackSize - object_offset)
  //
  // In practice, the frame info's getObjectOffset returns offset from SP.
  // So: FrameIndex ref = bp + (object_offset - StackSize)
  // But since bp = sp + StackSize (after prologue):
  //   object addr = sp + object_offset = bp - StackSize + object_offset = bp + (object_offset - StackSize)

  if (ST.hasSAF()) {
    int StackSize = MFI.getStackSize();
    int ObjectOffset = MFI.getObjectOffset(FI);
    int Offset = ObjectOffset - StackSize;
    // For fixed objects (arguments), the offset is already relative to SP
    // before the prologue, so we need to add back the saved bp.
    if (MFI.isFixedObjectIndex(FI)) {
      Offset += 2; // adjust for pushed bp
    }
    return StackOffset::getFixed(Offset);
  }

  // Without SAF: no frame setup, just use the raw offset
  return StackOffset::getFixed(MFI.getObjectOffset(FI));
}

bool ETCAFrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI) const {
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  if (!ST.hasSAF())
    return true; // No callee-saved registers without SAF

  MachineFrameInfo &MFI = MF.getFrameInfo();
  unsigned RegWidth = ST.getRegWidth();
  unsigned SlotSize = RegWidth / 8;

  for (auto &CS : CSI) {
    unsigned Reg = CS.getReg();
    (void)TRI->getMinimalPhysRegClassLLT(
        Reg, LLT::scalar(RegWidth));
    int FI = MFI.CreateStackObject(SlotSize, Align(SlotSize), true);
    CS.setFrameIdx(FI);
  }
  return true;
}
