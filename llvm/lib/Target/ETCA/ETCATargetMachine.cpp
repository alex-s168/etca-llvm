//===-- ETCATargetMachine.cpp - Define TargetMachine for ETCA ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the info about ETCA target spec.
//
// ETCA uses GlobalISel exclusively.  The pass pipeline is:
//   IRTranslator → Legalizer → RegBankSelect → InstructionSelect
//
// No SDAG fallback is registered.
//===----------------------------------------------------------------------===//

#include "ETCATargetMachine.h"

#include "ETCA.h"
#include "TargetInfo/ETCATargetInfo.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/InitializePasses.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Target/TargetOptions.h"
#include <optional>

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeETCATarget() {
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeGlobalISel(PR);
  initializeETCASelectExpandPass(PR);
  RegisterTargetMachine<ETCATargetMachine> X(getTheETCATarget());
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

/// Build a DataLayout string from CPU name.
/// Delegates to the authoritative buildDataLayoutString in ETCASubtarget.
static std::string computeDataLayout(StringRef CPU) {
  unsigned WordSize = 16;
  unsigned PtrSize = 16;

  if (CPU == "etca32") {
    WordSize = 32; PtrSize = 32;
  } else if (CPU == "etca32p64") {
    WordSize = 32; PtrSize = 64;
  } else if (CPU == "etca64p32") {
    WordSize = 64; PtrSize = 32;
  } else if (CPU == "etca64") {
    WordSize = 64; PtrSize = 64;
  }

  return ETCASubtarget::buildDataLayoutString(WordSize, PtrSize);
}

ETCATargetMachine::ETCATargetMachine(const Target &TheTarget,
                                     const Triple &TargetTriple,
                                     StringRef Cpu, StringRef FeatureString,
                                     const TargetOptions &Options,
                                     std::optional<Reloc::Model> RM,
                                     std::optional<CodeModel::Model> CodeModel,
                                     CodeGenOptLevel OptLevel, bool JIT)
    : CodeGenTargetMachineImpl(TheTarget,
                               computeDataLayout(Cpu),
                               TargetTriple, Cpu, FeatureString, Options,
                               getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CodeModel,
                                                     CodeModel::Small),
                               OptLevel),
      Subtarget(TargetTriple, Cpu, FeatureString, *this, Options, CodeModel,
                OptLevel) {
  initAsmInfo();
  // ETCA requires GlobalISel (no SDAG fallback).
  setGlobalISel(true);
}

namespace {
class ETCAPassConfig : public TargetPassConfig {
public:
  ETCAPassConfig(ETCATargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  /// GlobalISel-only: no SDAG.
  bool addInstSelector() override {
    return false;
  }

  /// Add GISel passes: IRTranslator → Legalizer → RegBankSelect → InstructionSelect
  bool addIRTranslator() override {
    addPass(new IRTranslator(getOptLevel()));
    return false;
  }

  bool addLegalizeMachineIR() override {
    addPass(new Legalizer());
    return false;
  }

  bool addRegBankSelect() override {
    addPass(new RegBankSelect());
    return false;
  }

  bool addGlobalInstructionSelect() override {
    addPass(new InstructionSelect());
    return false;
  }

  void addPreRegAlloc() override {
    addPass(createETCASelectExpandPass());
    TargetPassConfig::addPreRegAlloc();
  }
};
} // namespace

TargetPassConfig *ETCATargetMachine::createPassConfig(PassManagerBase &PM) {
  return new ETCAPassConfig(*this, PM);
}
