//===-- EtcaMCAsmInfo.cpp - Etca asm properties -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the EtcaMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "EtcaMCAsmInfo.h"

#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void EtcaMCAsmInfo::anchor() {}

EtcaMCAsmInfo::EtcaMCAsmInfo(const Triple & /*TheTriple*/,
                               const MCTargetOptions &Options) {
  IsLittleEndian = false;
  PrivateGlobalPrefix = ".L";
  WeakRefDirective = "\t.weak\t";
  ExceptionsType = ExceptionHandling::DwarfCFI;

  // Etca assembly requires ".section" before ".bss"
  UsesELFSectionDirectiveForBSS = true;

  CommentString = ";";

  // Target supports emission of debugging information.
  SupportsDebugInformation = true;

  // Set the instruction alignment. Currently used only for address adjustment
  // in dwarf generation.
  MinInstAlignment = 4;
}
