//===- ETCAFrameLowering.h - ETCA Frame Lowering ----------------*- C++ -*-===//
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
//   push bp
//   bp = sp
// and torn down with:
//   sp = bp
//   pop bp
//
// Without SAF, there is no stack frame — functions can't call each other.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_ETCAFRAMELOWERING_H
#define LLVM_LIB_TARGET_ETCA_ETCAFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class ETCASubtarget;

class ETCAFrameLowering : public TargetFrameLowering {
public:
  explicit ETCAFrameLowering(Align StackAlign)
      : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, StackAlign,
                            0) {}

  /// Whether the function needs a frame pointer.
  /// With SAF, we use r5 (bp) as the frame pointer.
  bool hasFPImpl(const MachineFunction &MF) const override;

  /// Emit prologue code (push bp; bp = sp; allocate stack).
  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  /// Emit epilogue code (deallocate stack; sp = bp; pop bp; ret).
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  /// Determine the offsets of FrameIndex references.
  StackOffset getFrameIndexReference(const MachineFunction &MF, int FI,
                                     Register &FrameReg) const override;

  /// Assign fixed slot indices for callee-saved registers.
  bool
  assignCalleeSavedSpillSlots(MachineFunction &MF,
                              const TargetRegisterInfo *TRI,
                              std::vector<CalleeSavedInfo> &CSI) const override;

  /// Save callee-saved registers using PUSH instead of per-register
  /// store-to-stack-slot.  Returns true (handled); the actual PUSH
  /// instructions are emitted in emitPrologue.
  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override {
    return true;
  }

  /// Restore callee-saved registers using POP instead of per-register
  /// load-from-stack-slot.  Returns true (handled); the actual POP
  /// instructions are emitted in emitEpilogue.
  bool
  restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI,
                              MutableArrayRef<CalleeSavedInfo> CSI,
                              const TargetRegisterInfo *TRI) const override {
    return true;
  }
};

} // namespace llvm

#endif
