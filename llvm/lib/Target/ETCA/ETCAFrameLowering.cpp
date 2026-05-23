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
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  if (!ST.hasSAF())
    return false;

  const MachineFrameInfo &MFI = MF.getFrameInfo();

  // Respect user request (-fno-omit-frame-pointer / frame-pointer=all).
  // This is the standard hook that every target calls first.
  if (MF.getTarget().Options.DisableFramePointerElim(MF))
    return true;

  // Frame pointer is required when the function actually needs stack
  // access that can't be sp-relative:
  //   1. Variable-sized stack objects (alloca/VLA)
  //   2. Frame address has been taken (e.g., by builtin frame address)
  //   3. The function has calls (needed for unwind/debug info)
  //   4. The stack has been adjusted (callee-saved spills, locals)
  if (MFI.hasVarSizedObjects())
    return true;
  if (MFI.isFrameAddressTaken())
    return true;
  if (MFI.hasCalls())
    return true;
  if (MFI.getStackSize() > 0)
    return true;

  // Leaf functions with no stack accesses don't need a frame pointer.
  return false;
}

void ETCAFrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  if (!ST.hasSAF())
    return;
  // Skip frame setup if the function doesn't need a frame pointer.
  if (!hasFP(MF))
    return;

  MachineBasicBlock::iterator MBBI = MBB.begin();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetInstrInfo &TII = *ST.getInstrInfo();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  // SAF prologue:
  //   push r5          ; save old base pointer
  //   mov r5, r6       ; bp = sp
  //   push rN          ; save callee-saved registers (via PUSH)
  //   sub r6, N        ; allocate remaining N bytes for locals
  //
  // With this layout, sp = bp - StackSize after the sub (where StackSize
  // includes CSR slot sizes from assignCalleeSavedSpillSlots).  The PUSH
  // instructions decrement sp for the CSR registers and store them into
  // what will become the bottom of the allocated frame.

  unsigned RegWidth = ST.getRegWidth();
  unsigned SlotSize = RegWidth / 8;

  // 1. push r5 (save old base pointer / frame link)
  unsigned PushOpc;
  unsigned PopOpc;
  switch (RegWidth) {
  case 64:
    PushOpc = PUSH64;
    PopOpc = POP64;
    break;
  case 32:
    PushOpc = PUSH32;
    PopOpc = POP32;
    break;
  default:
    PushOpc = PUSH;
    PopOpc = POP;
    break;
  }
  BuildMI(MBB, MBBI, DL, TII.get(PushOpc)).addReg(R5);

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

  // 3. Push callee-saved registers using PUSH (avoids verbose MOVZ+ADDI+STORE)
  unsigned CSRPushSize = 0;
  for (auto &CS : MFI.getCalleeSavedInfo()) {
    // R5 (bp) was already pushed in step 1; R6 (sp) is never saved.
    if (CS.getReg() == ETCA::R5 || CS.getReg() == ETCA::R6)
      continue;
    BuildMI(MBB, MBBI, DL, TII.get(PushOpc)).addReg(CS.getReg());
    CSRPushSize += SlotSize;
  }

  // 4. Allocate remaining stack (locals, other spills).  The CSR slots
  //    created by assignCalleeSavedSpillSlots are accounted for in
  //    StackSize, but since the PUSH instructions already allocated that
  //    space, we subtract CSRPushSize from the alloc amount here.
  int StackSize = MFI.getStackSize();
  int AdjustedStackSize = StackSize - static_cast<int>(CSRPushSize);
  if (AdjustedStackSize > 0) {
    // sub r6, AdjustedStackSize — RI format: [dst, src1(tied), imm]
    BuildMI(MBB, MBBI, DL, TII.get(SubOpc), R6)
        .addReg(R6)
        .addImm(AdjustedStackSize);
  }
}

void ETCAFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  if (!ST.hasSAF())
    return;
  // Skip frame teardown if the function doesn't need a frame pointer.
  if (!hasFP(MF))
    return;

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetInstrInfo &TII = *ST.getInstrInfo();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  // SAF epilogue (insert before the return instruction):
  //   mov r6, r5       ; sp = bp
  //   sub r6, N        ; adjust sp down to reach pushed CSRs
  //   pop rN           ; restore callee-saved registers
  //   pop r5           ; restore old base pointer

  unsigned RegWidth = ST.getRegWidth();
  unsigned SlotSize = RegWidth / 8;

  unsigned PushOpc;
  unsigned PopOpc;
  switch (RegWidth) {
  case 64:
    PushOpc = PUSH64;
    PopOpc = POP64;
    break;
  case 32:
    PushOpc = PUSH32;
    PopOpc = POP32;
    break;
  default:
    PushOpc = PUSH;
    PopOpc = POP;
    break;
  }

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

  // 1. mov r6, r5 (sp = bp) — undo the local/spill allocation
  BuildMI(MBB, MBBI, DL, TII.get(MovOpc), R6).addReg(R5);

  // 2. Adjust sp down to reach the pushed CSRs.
  unsigned CSRPushSize = 0;
  for (auto &CS : MFI.getCalleeSavedInfo()) {
    if (CS.getReg() == ETCA::R5 || CS.getReg() == ETCA::R6)
      continue;
    CSRPushSize += SlotSize;
  }
  if (CSRPushSize > 0) {
    int64_t Remaining = CSRPushSize;
    while (Remaining > 0) {
      int64_t Step = std::min<int64_t>(Remaining, 15);
      BuildMI(MBB, MBBI, DL, TII.get(SubOpc), R6).addReg(R6).addImm(Step);
      Remaining -= Step;
    }
  }

  // 3. Pop callee-saved registers (reverse of push order)
  const auto &CSI = MFI.getCalleeSavedInfo();
  for (auto It = CSI.rbegin(); It != CSI.rend(); ++It) {
    if (It->getReg() == ETCA::R5 || It->getReg() == ETCA::R6)
      continue;
    BuildMI(MBB, MBBI, DL, TII.get(PopOpc), It->getReg());
  }

  // 4. pop r5 (restore old base pointer)
  BuildMI(MBB, MBBI, DL, TII.get(PopOpc), R5);
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
      // SP (before the prologue).  After the prologue, bp = initial_SP -
      // SlotSize (pushed old bp), so the offset from bp is:
      //   addr = initial_SP + ObjectOffset = (bp + SlotSize) + ObjectOffset
      //   offset_from_bp = ObjectOffset + SlotSize
      unsigned RegWidth = ST.getRegWidth();
      unsigned SlotSize = RegWidth / 8;
      return StackOffset::getFixed(ObjectOffset + SlotSize);
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
    int FI = MFI.CreateStackObject(SlotSize, Align(SlotSize), true);
    CS.setFrameIdx(FI);
  }
  return true;
}

MachineBasicBlock::iterator
ETCAFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  const TargetInstrInfo &TII = *ST.getInstrInfo();
  DebugLoc DL = MI->getDebugLoc();
  unsigned Opc = MI->getOpcode();
  bool IsDestroy = (Opc == TII.getCallFrameDestroyOpcode());

  // If we have a reserved call frame, the space was pre-allocated in
  // the prologue; just erase the pseudo.
  if (hasReservedCallFrame(MF)) {
    return MBB.erase(MI);
  }

  // Otherwise, emit the actual SP adjustment using SUBI/ADDI.
  // RI instructions have only 5-bit immediates (max 31), so we may
  // need multiple instructions for large call frames.
  int64_t Amount = MI->getOperand(0).getImm();
  if (Amount == 0) {
    return MBB.erase(MI);
  }

  unsigned RegWidth = ST.getRegWidth();
  unsigned SubOpc, AddOpc;
  switch (RegWidth) {
  case 64:
    SubOpc = SUBI64;
    AddOpc = ADDI64;
    break;
  case 32:
    SubOpc = SUBI32;
    AddOpc = ADDI32;
    break;
  default:
    SubOpc = SUBI16;
    AddOpc = ADDI16;
    break;
  }

  // Break up large adjustments into max-31-byte chunks (5-bit immediate
  // gives range 0-31, but we use 31 as the max since negative immediates
  // are not needed for positive offsets).
  const int64_t MaxImm = 31;
  int64_t Remaining = Amount;
  while (Remaining > 0) {
    int64_t Step = std::min<int64_t>(Remaining, MaxImm);
    if (IsDestroy) {
      // ADJCALLSTACKUP -> ADDI R6, Step  (restore SP after call)
      BuildMI(MBB, MI, DL, TII.get(AddOpc), ETCA::R6)
          .addReg(ETCA::R6)
          .addImm(Step);
    } else {
      // ADJCALLSTACKDOWN -> SUBI R6, Step  (allocate call frame)
      BuildMI(MBB, MI, DL, TII.get(SubOpc), ETCA::R6)
          .addReg(ETCA::R6)
          .addImm(Step);
    }
    Remaining -= Step;
  }

  return MBB.erase(MI);
}
