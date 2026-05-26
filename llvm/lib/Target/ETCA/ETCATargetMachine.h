//===-- ETCATargetMachine.h - Define TargetMachine for ETCA ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the ETCA specific subclass of TargetMachine.
// ETCA uses GlobalISel exclusively — no SDAG fallback.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_ETCATARGETMACHINE_H
#define LLVM_LIB_TARGET_ETCA_ETCATARGETMACHINE_H

#include "ETCASubtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include <optional>

namespace llvm {

class ETCATargetMachine : public CodeGenTargetMachineImpl {
  ETCASubtarget Subtarget;
  TargetLoweringObjectFileELF TLOF;

public:
  ETCATargetMachine(const Target &TheTarget, const Triple &TargetTriple,
                    StringRef Cpu, StringRef FeatureString,
                    const TargetOptions &Options,
                    std::optional<Reloc::Model> RM,
                    std::optional<CodeModel::Model> CodeModel,
                    CodeGenOptLevel OptLevel, bool JIT);

  const ETCASubtarget *
  getSubtargetImpl(const llvm::Function & /*Fn*/) const override {
    return &Subtarget;
  }

  const ETCASubtarget *getSubtargetImpl() const { return &Subtarget; }

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return const_cast<TargetLoweringObjectFileELF *>(&TLOF);
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_ETCATARGETMACHINE_H
