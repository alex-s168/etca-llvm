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
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Target/TargetOptions.h"
#include <optional>

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeETCATarget() {
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeGlobalISel(PR);
  initializeETCASelectExpandPass(PR);
  initializeETCAEliminateIdentityMovesPass(PR);
  RegisterTargetMachine<ETCATargetMachine> X(getTheETCATarget());
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

/// Build a DataLayout string from the feature string.
/// Parses -mattr features for +64bit/+32bit (word size) and +ptr64/+ptr32
/// (pointer size); defaults to 16+16.
static std::string computeDataLayout(StringRef FS) {
  unsigned WordSize = 16;
  unsigned PtrSize = 16;

  // Parse feature string for word/pointer size features.
  // The FS format is "+feat1,-feat2,+feat3,..."
  SmallVector<StringRef, 8> Features;
  FS.split(Features, ',', -1, false);
  for (StringRef F : Features) {
    F = F.trim();
    if (F == "+64bit")
      WordSize = 64;
    else if (F == "+32bit")
      WordSize = 32;
    else if (F == "+ptr64")
      PtrSize = 64;
    else if (F == "+ptr32")
      PtrSize = 32;
  }

  return ETCASubtarget::buildDataLayoutString(WordSize, PtrSize);
}

ETCATargetMachine::ETCATargetMachine(const Target &TheTarget,
                                     const Triple &TargetTriple, StringRef Cpu,
                                     StringRef FeatureString,
                                     const TargetOptions &Options,
                                     std::optional<Reloc::Model> RM,
                                     std::optional<CodeModel::Model> CodeModel,
                                     CodeGenOptLevel OptLevel, bool JIT)
    : CodeGenTargetMachineImpl(
          TheTarget, computeDataLayout(FeatureString), TargetTriple, Cpu,
          FeatureString, Options, getEffectiveRelocModel(RM),
          getEffectiveCodeModel(CodeModel, CodeModel::Small), OptLevel),
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
  bool addInstSelector() override { return false; }

  /// Add GISel passes: IRTranslator → Legalizer → RegBankSelect →
  /// InstructionSelect
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

  /// Skip the Machine Control Flow Optimizer (BranchFolding) and
  /// Machine Block Placement (reorder blocks) passes which do not
  /// understand ETCa's PUSH/POP stack pseudo-instructions and crash
  /// when optimizing functions containing them.
  void addMachineLateOptimization() override {
    addPass(&MachineLateInstrsCleanupID);
    // Skip BranchFolderPass — crashes on ETCa's PUSH/POP.
    // addPass(&BranchFolderPassID);
    if (!TM->requiresStructuredCFG())
      addPass(&TailDuplicateLegacyID);
    addPass(&MachineCopyPropagationID);
    // Eliminate MOVZ/MOVS identity copies ($rX = MOVZ $rX) that arise
    // from G_TRUNC/G_ZEXT/G_SEXT temp vregs landing on the same physical
    // register as the source, or from SELECT_Pseudo expansions of
    // identical true/false values.
    addPass(createETCAEliminateIdentityMovesPass());
  }

  void addBlockPlacement() override {
    // Skip MachineBlockPlacement — crashes on ETCa's PUSH/POP.
    // addPass(&MachineBlockPlacementID);
  }
};
} // namespace

TargetPassConfig *ETCATargetMachine::createPassConfig(PassManagerBase &PM) {
  return new ETCAPassConfig(*this, PM);
}
