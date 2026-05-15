//===- ETCAFrameLowering.cpp - ETCA Frame Lowering
//-------------------------===//
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

  unsigned RegWidth = ST.getRegWidth();

  // 1. push r5
  BuildMI(MBB, MBBI, DL, TII.get(PUSH)).addReg(R5);
  unsigned MovOpc;
  switch (RegWidth) {
  case 64:
    MovOpc = MOVZ64;
    break;
  case 32:
    MovOpc = MOVZ32;
    break;
  default:
    MovOpc = MOVZ16;
    break;
  }

  unsigned SubOpc;
  switch (RegWidth) {
  case 64:
    SubOpc = SUBI64;
    break;
  case 32:
    SubOpc = SUBI32;
    break;
  default:
    SubOpc = SUBI16;
    break;
  }

  // 2. mov r5, r6 (copy sp to bp) — RR format (non-tied): [dst, src]
  BuildMI(MBB, MBBI, DL, TII.get(MovOpc), R5).addReg(R6);

  // 3. Allocate stack for locals
  int StackSize = MFI.getStackSize();
  if (StackSize > 0) {
    // sub r6, StackSize — RI format: [dst, src1(tied), imm]
    BuildMI(MBB, MBBI, DL, TII.get(SubOpc), R6).addReg(R6).addImm(StackSize);
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

  unsigned RegWidth = ST.getRegWidth();
  unsigned MovOpc;
  switch (RegWidth) {
  case 64:
    MovOpc = MOVZ64;
    break;
  case 32:
    MovOpc = MOVZ32;
    break;
  default:
    MovOpc = MOVZ16;
    break;
  }

  // 1. mov r6, r5 — RR format (non-tied): [dst, src]
  BuildMI(MBB, MBBI, DL, TII.get(MovOpc), R6).addReg(R5);

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
  //   bp = sp (after prologue: push r5, mov r5, r6)
  //   sp = bp - StackSize (after: sub r6, StackSize)
  //
  // Stack frame layout (grows down):
  //     [old bp]      <- bp + 0     (saved by push r5)
  //     [spills/locals] <- bp - 2 .. bp - StackSize
  //                       <- sp = bp - StackSize
  //
  // The PEI pass assigns ObjectOffset = -Offset where Offset is the
  // distance from SP (after prologue) going downward.  But for ETCa we
  // want objects to live WITHIN the allocated region [sp, bp], i.e. at
  // bp-2, bp-4, ..., bp-StackSize.  Since the PEI assigns -
  // sequentially-increasing values, those values happen to be exactly
  // the bp-relative offsets we need (negated).
  //
  // Example: 3 objects of size 2 → offsets -2, -4, -6 from SP.
  //   StackSize = 6, sp = bp - 6.
  //   Using Offset = ObjectOffset (not ObjectOffset - StackSize):
  //     object at SP-2 → bp-2   (bp + (-2))
  //     object at SP-4 → bp-4   (bp + (-4))
  //     object at SP-6 → bp-6   (bp + (-6))
  //   All within [bp-6, bp-0].

  if (ST.hasSAF()) {
    int ObjectOffset = MFI.getObjectOffset(FI);

    if (MFI.isFixedObjectIndex(FI)) {
      // Fixed objects (incoming arguments) are at offsets from the initial
      // SP (before the prologue).  After the prologue, bp = initial_SP - 2
      // (pushed old bp), so the offset from bp is:
      //   addr = initial_SP + ObjectOffset = (bp + 2) + ObjectOffset
      //   offset_from_bp = ObjectOffset + 2
      return StackOffset::getFixed(ObjectOffset + 2);
    }

    // Non-fixed objects (locals, spills):
    //   PEI assigns ObjectOffset = -distance_from_SP (downward).
    //   These values directly give the bp-relative offset since
    //   sp = bp - StackSize and the offsets are sequential:
    //     bp_offset = ObjectOffset  (no StackSize subtraction)
    //   This places all objects within [bp-StackSize, bp-0].
    int Offset = ObjectOffset;
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
    (void)TRI->getMinimalPhysRegClassLLT(Reg, LLT::scalar(RegWidth));
    int FI = MFI.CreateStackObject(SlotSize, Align(SlotSize), true);
    CS.setFrameIdx(FI);
  }
  return true;
}
