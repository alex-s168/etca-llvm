//===- ETCARegisterInfo.h - ETCA Register Information ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the ETCA implementation of the TargetRegisterInfo class.
//
// ETCA register conventions (SAF ABI):
//   r0 (a0/v0) — argument/return value 0  (caller-saved)
//   r1 (a1/v1) — argument/return value 1  (caller-saved)
//   r2 (a2)    — argument 2                (caller-saved)
//   r3 (s0)    — callee-saved register
//   r4 (s1)    — callee-saved register
//   r5 (bp)    — base pointer              (callee-saved)
//   r6 (sp)    — stack pointer             (callee-saved)
//   r7 (ln)    — link register             (caller-saved)
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_ETCAREGISTERINFO_H
#define LLVM_LIB_TARGET_ETCA_ETCAREGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "ETCAGenRegisterInfo.inc"

namespace llvm {

class ETCASubtarget;

struct ETCARegisterInfo : public ETCAGenRegisterInfo {
public:
  ETCARegisterInfo(const ETCASubtarget &ST);

  const TargetRegisterClass *
  getPointerRegClass(unsigned Kind = 0) const override;

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;

  BitVector getReservedRegs(const MachineFunction &MF) const override;

  bool eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;

  bool requiresRegisterScavenging(const MachineFunction &MF) const override;

  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const override;

  Register getFrameRegister(const MachineFunction &MF) const override;

private:
  const ETCASubtarget &ST;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_ETCAREGISTERINFO_H
