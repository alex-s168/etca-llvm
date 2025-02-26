//===-- LanaiTargetInfo.cpp - Lanai Target Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/EtcaTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &llvm::getTheEtcaTarget() {
  static Target TheEtcaTarget;
  return TheEtcaTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEtcaTargetInfo() {
  RegisterTarget<Triple::etca> X(getTheEtcaTarget(), "etca", "Etca",
                                 "Etca");
}
