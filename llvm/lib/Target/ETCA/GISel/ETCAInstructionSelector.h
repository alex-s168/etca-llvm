//===- ETCAInstructionSelector.h - ETCA Instruction Selector ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_GISEL_ETCAINSTRUCTIONSELECTOR_H
#define LLVM_LIB_TARGET_ETCA_GISEL_ETCAINSTRUCTIONSELECTOR_H

#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"

namespace llvm {

class ETCATargetMachine;
class ETCASubtarget;

class ETCAInstructionSelector : public InstructionSelector {
public:
  ETCAInstructionSelector(const TargetMachine &TM,
                          const ETCASubtarget &ST,
                          const RegisterBankInfo &RBI);

  bool select(MachineInstr &MI) override;
  void setupGeneratedPerFunctionState(MachineFunction &MF) override;
  static const char *getName() { return "ETCAInstructionSelector"; }

private:
  const TargetMachine &TM;
  const ETCASubtarget &ST;
  const RegisterBankInfo &RBI;
  const TargetRegisterInfo &TRI;

  /// Get the MOVS opcode for a given destination type size.
  static unsigned getMovsOpc(unsigned Size);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_GISEL_ETCAINSTRUCTIONSELECTOR_H
