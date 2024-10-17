//====-- EtcaCode.h - Etca operator encoding ----------------------------====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The encoding for Etca operands
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_ETCACODE_H
#define LLVM_LIB_TARGET_ETCA_ETCACODE_H

#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {
namespace ETCA {
enum OpCode {
  ADD  = 0b0000,
  SUB  = 0b0001,
  RSUB = 0b0010,
  CMP  = 0b0011,
  OR   = 0b0100,
  XOR  = 0b0101,
  AND  = 0b0110,
  TEST = 0b0111,
  MOVZ = 0b1000,
  MOVS = 0b1001,
  LOAD = 0b1010,
  STORE   = 0b1011,
  SLO     = 0b1100,
  READCR  = 0b1110,
  WRITECR = 0b1111,

  // Indicates an unknown/unsupported operator
  UNKNOWN = 0xFF,
};

inline OpCode getOpCode(unsigned Val) {
  return static_cast<OpCode>(Val);
}

inline static const char *etcaOpCodeToString(unsigned OpCode) {
  switch (getOpCode(OpCode)) {
  case ADD:
    return "add";
  case SUB:
    return "sub";
  case RSUB:
    return "rsub";
  case CMP:
    return "cmp";
  case OR:
    return "or";
  case XOR:
    return "xor";
  case AND:
    return "and";
  case TEST:
    return "test";
  case MOVZ:
    return "movz";
  case MOVS:
    return "movs";
  case LOAD:
    return "load";
  case STORE:
    return "store";
  case SLO:
    return "slo";
  case READCR:
    return "readcr";
  case WRITECR:
    return "writecr";
  default:
    llvm_unreachable("Invalid ALU code.");
  }
}

inline static OpCode stringToEtcaOpCode(StringRef S) {
  return StringSwitch<OpCode>(S)
      .Case("add", ADD)
      .Case("sub", SUB)
      .Case("rsub", RSUB)
      .Case("cmp", CMP)
      .Case("or", OR)
      .Case("xor", XOR)
      .Case("and", AND)
      .Case("test", TEST)
      .Case("movz", MOVZ)
      .Case("movs", MOVS)
      .Case("load", LOAD)
      .Case("store", STORE)
      .Case("slo", SLO)
      .Case("readcr", READCR)
      .Case("writecr", WRITECR)
      .Default(UNKNOWN);
}
} // namespace ETCA
} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_ETCACODE_H
