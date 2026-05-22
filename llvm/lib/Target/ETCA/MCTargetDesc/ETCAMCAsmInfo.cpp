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

ETCAMCAsmInfo::ETCAMCAsmInfo(const Triple &TT, const MCTargetOptions &Options,
                             unsigned PtrSize)
    : MCAsmInfoELF(Options) {
  // Pointer size is determined by the caller (see createETCAMCAsmInfo).
  // The triple OS name suffix encodes the desired pointer size:
  //   etca-unknown-elf    → 16-bit (default, base ISA)
  //   etca-unknown-elf32  → 32-bit (DWAS extension)
  //   etca-unknown-elf64  → 64-bit (QWAS extension)
  CodePointerSize = PtrSize / 8;
  CalleeSaveStackSlotSize = PtrSize / 8;
  MinInstAlignment = 2;
  MaxInstLength = 2;

  // Comment string
  CommentString = ";";
}
