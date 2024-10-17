//===- EtcaSubtarget.cpp - Etca Subtarget Information -----------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Etca specific subclass of TargetSubtarget.
//
//===----------------------------------------------------------------------===//

#include "EtcaSubtarget.h"

#include "Etca.h"

#define DEBUG_TYPE "etca-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "EtcaGenSubtargetInfo.inc"

using namespace llvm;

void EtcaSubtarget::initSubtargetFeatures(StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "generic";

  ParseSubtargetFeatures(CPUName, /*TuneCPU*/ CPUName, FS);
}

EtcaSubtarget &EtcaSubtarget::initializeSubtargetDependencies(StringRef CPU,
                                                              StringRef FS) {
  initSubtargetFeatures(CPU, FS);
  return *this;
}

EtcaSubtarget::EtcaSubtarget(const Triple &TargetTriple, StringRef Cpu,
                             StringRef FeatureString, const TargetMachine &TM,
                             const TargetOptions & /*Options*/,
                             CodeModel::Model /*CodeModel*/,
                             CodeGenOptLevel /*OptLevel*/)
    : EtcaGenSubtargetInfo(TargetTriple, Cpu, /*TuneCPU*/ Cpu, FeatureString),
      FrameLowering(initializeSubtargetDependencies(Cpu, FeatureString)),
      TLInfo(TM, *this) {}
