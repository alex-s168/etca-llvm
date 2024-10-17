//===-- Etca.h - Top-level interface for Etca representation --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// Etca back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LANAI_LANAI_H
#define LLVM_LIB_TARGET_LANAI_LANAI_H

#include "llvm/Pass.h"

namespace llvm {
class FunctionPass;
class EtcaTargetMachine;

// createEtcaISelDag - This pass converts a legalized DAG into a
// Etca-specific DAG, ready for instruction scheduling.
FunctionPass *createEtcaISelDag(EtcaTargetMachine &TM);

} // namespace llvm

#endif // LLVM_LIB_TARGET_LANAI_LANAI_H
