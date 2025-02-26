//===-- EtcaMCTargetDesc.cpp - Etca Target Descriptions -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Etca specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "EtcaMCTargetDesc.h"
#include "EtcaInstPrinter.h"
#include "EtcaMCAsmInfo.h"
#include "TargetInfo/EtcaTargetInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/TargetParser/Triple.h"
#include <cstdint>
#include <string>

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "EtcaGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "EtcaGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "EtcaGenRegisterInfo.inc"

using namespace llvm;

static MCInstrInfo *createEtcaMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitEtcaMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createEtcaMCRegisterInfo(const Triple & /*TT*/) {
  MCRegisterInfo *X = new MCRegisterInfo();
  return X;
}

static MCSubtargetInfo *
createEtcaMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "generic";

  return createEtcaMCSubtargetInfoImpl(TT, CPUName, /*TuneCPU*/ CPUName, FS);
}

static MCInstPrinter *createEtcaMCInstPrinter(const Triple & /*T*/,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new EtcaInstPrinter(MAI, MII, MRI);
  return nullptr;
}

static MCRelocationInfo *createEtcaElfRelocation(const Triple &TheTriple,
                                                  MCContext &Ctx) {
  return createMCRelocationInfo(TheTriple, Ctx);
}

namespace {

class EtcaMCInstrAnalysis : public MCInstrAnalysis {
public:
  explicit EtcaMCInstrAnalysis(const MCInstrInfo *Info)
      : MCInstrAnalysis(Info) {}

  bool evaluateBranch(const MCInst &Inst, uint64_t Addr, uint64_t Size,
                      uint64_t &Target) const override {
    if (Inst.getNumOperands() == 0)
      return false;
    if (!isConditionalBranch(Inst) && !isUnconditionalBranch(Inst) &&
        !isCall(Inst))
      return false;

    if (Info->get(Inst.getOpcode()).operands()[0].OperandType ==
        MCOI::OPERAND_PCREL) {
      int64_t Imm = Inst.getOperand(0).getImm();
      Target = Addr + Size + Imm;
      return true;
    } else {
      int64_t Imm = Inst.getOperand(0).getImm();

      // Skip case where immediate is 0 as that occurs in file that isn't linked
      // and the branch target inferred would be wrong.
      if (Imm == 0)
        return false;

      Target = Imm;
      return true;
    }
  }
};

} // end anonymous namespace

static MCInstrAnalysis *createEtcaInstrAnalysis(const MCInstrInfo *Info) {
  return new EtcaMCInstrAnalysis(Info);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEtcaTargetMC() {
  // Register the MC asm info.
  RegisterMCAsmInfo<EtcaMCAsmInfo> X(getTheEtcaTarget());

  // Register the MC instruction info.
  TargetRegistry::RegisterMCInstrInfo(getTheEtcaTarget(),
                                      createEtcaMCInstrInfo);

  // Register the MC register info.
  TargetRegistry::RegisterMCRegInfo(getTheEtcaTarget(),
                                    createEtcaMCRegisterInfo);

  // Register the MC subtarget info.
  TargetRegistry::RegisterMCSubtargetInfo(getTheEtcaTarget(),
                                          createEtcaMCSubtargetInfo);

  // Register the MC code emitter
  TargetRegistry::RegisterMCCodeEmitter(getTheEtcaTarget(),
                                        createEtcaMCCodeEmitter);

  // Register the ASM Backend
  TargetRegistry::RegisterMCAsmBackend(getTheEtcaTarget(),
                                       createEtcaMCAsmBackend);

  // Register the MCInstPrinter.
  TargetRegistry::RegisterMCInstPrinter(getTheEtcaTarget(),
                                        createEtcaMCInstPrinter);

  // Register the MC relocation info.
  TargetRegistry::RegisterMCRelocationInfo(getTheEtcaTarget(),
                                           createEtcaElfRelocation);

  // Register the MC instruction analyzer.
  TargetRegistry::RegisterMCInstrAnalysis(getTheEtcaTarget(),
                                          createEtcaInstrAnalysis);
}
