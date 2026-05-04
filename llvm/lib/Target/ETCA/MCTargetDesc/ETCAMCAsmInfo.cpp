//===-- ETCAMCAsmInfo.cpp - ETCA Asm Properties ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ETCAMCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void anchor() {}

ETCAMCAsmInfo::ETCAMCAsmInfo(const Triple &TT, const MCTargetOptions &Options)
    : MCAsmInfoELF(Options) {
  // Default to 16-bit pointers (base ISA).
  // The subtarget can override these values at runtime.
  CodePointerSize = 2;
  CalleeSaveStackSlotSize = 2;
  MinInstAlignment = 2;
  MaxInstLength = 2;

  // Comment string
  CommentString = ";";
}
