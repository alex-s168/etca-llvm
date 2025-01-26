//===-- EtcaTargetMachine.cpp - Define TargetMachine for Etca ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the info about Etca target spec.
//
//===----------------------------------------------------------------------===//


#include "EtcaTargetMachine.h"
#include "TargetInfo/EtcaTargetInfo.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include <optional>

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEtcaTarget() {
  // Register the target.
  RegisterTargetMachine<EtcaTargetMachine> A(getTheEtcaTarget());
}

static std::string computeDataLayout(const Triple &TT, StringRef CPU,
                                     const TargetOptions &Options,
                                     bool IsLittle) {
  std::string Ret = "e-m:e-p:16:16-i8:8:16-i16:16:16-i32:32:32-n8:16";
  return Ret;
}

static Reloc::Model getEffectiveRelocModel(bool JIT,
                                           std::optional<Reloc::Model> RM) {
  if (!RM || JIT)
     return Reloc::Static;
  return *RM;
}

EtcaTargetMachine::EtcaTargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS,
                                     const TargetOptions &Options,
                                     std::optional<Reloc::Model> RM,
                                     std::optional<CodeModel::Model> CM,
                                     CodeGenOptLevel OL, bool JIT,
                                     bool IsLittle)
    : LLVMTargetMachine(T, computeDataLayout(TT, CPU, Options, IsLittle), TT,
                        CPU, FS, Options, getEffectiveRelocModel(JIT, RM),
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()) {
  initAsmInfo();
}

EtcaTargetMachine::EtcaTargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS,
                                     const TargetOptions &Options,
                                     std::optional<Reloc::Model> RM,
                                     std::optional<CodeModel::Model> CM,
                                     CodeGenOptLevel OL, bool JIT)
    : EtcaTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL, JIT, true) {}

const EtcaSubtarget *
EtcaTargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  auto CPU = CPUAttr.isValid() ? CPUAttr.getValueAsString().str() : TargetCPU;
  auto FS = FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;

  auto &I = SubtargetMap[CPU + FS];
  if (!I) {
    // This needs to be done before we create a new subtarget since any
    // creation will depend on the TM and the code generation flags on the
    // function that reside in TargetOptions.
    resetTargetOptions(F);
    I = std::make_unique<EtcaSubtarget>(TargetTriple, CPU, FS, *this, Options, getCodeModel(), getOptLevel());
  }
  return I.get();
}

namespace {
/// Etca Code Generator Pass Configuration Options.
class EtcaPassConfig : public TargetPassConfig {
public:
  EtcaPassConfig(EtcaTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  EtcaTargetMachine &getEtcaTargetMachine() const {
    return getTM<EtcaTargetMachine>();
  }

  bool addInstSelector() override;
  bool addIRTranslator() override;
  bool addLegalizeMachineIR() override;
  bool addRegBankSelect() override;
  bool addGlobalInstructionSelect() override;
};
} // end anonymous namespace

bool EtcaPassConfig::addLegalizeMachineIR() {
  addPass(new Legalizer());
  return false;
}

bool EtcaPassConfig::addInstSelector() {
  addPass(createEtcaISelDag(getEtcaTargetMachine(), getOptLevel()));
  return false;
}

bool EtcaPassConfig::addIRTranslator() {
  addPass(new IRTranslator(getOptLevel()));
  return false;
}

bool EtcaPassConfig::addRegBankSelect() {
  addPass(new RegBankSelect());
  return false;
}

bool EtcaPassConfig::addGlobalInstructionSelect() {
  addPass(new InstructionSelect(getOptLevel()));
  return false;
}

TargetPassConfig *EtcaTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new EtcaPassConfig(*this, PM);
}
