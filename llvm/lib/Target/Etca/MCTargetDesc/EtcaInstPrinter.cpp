//===-- EtcaInstPrinter.cpp - Convert Etca MCInst to asm syntax ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class prints an Etca MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#include "EtcaInstPrinter.h"
#include "EtcaMCExpr.h"
#include "MCTargetDesc/EtcaMCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "EtcaGenAsmWriter.inc"

void EtcaInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) const {
  OS << StringRef(getRegisterName(Reg)).lower();
}

void EtcaInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annotation,
                                 const MCSubtargetInfo & /*STI*/,
                                 raw_ostream &OS) {
  assert(false);
}

void EtcaInstPrinter::printOperand(const MCInst *MI, uint64_t ID, raw_ostream &O) {
  assert(false);
}

void EtcaInstPrinter::printImm5_AsmOperand(const MCInst *MI, uint64_t ID, raw_ostream &O) {
  assert(false);
}
