//===-- ETCA.h - Top-level interface for ETCA representation ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the
// LLVM ETCA back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_ETCA_H
#define LLVM_LIB_TARGET_ETCA_ETCA_H

#include "llvm/Pass.h"

namespace llvm {

class ETCATargetMachine;
class FunctionPass;
class PassRegistry;

void initializeETCAAsmPrinterPass(PassRegistry &);

/// Create the ETCA DAG-to-DAG instruction selector pass.
/// NOTE: This is only for development/testing. The primary path is GISel.
FunctionPass *createETCAISelDag(ETCATargetMachine &TM,
                                CodeGenOptLevel OptLevel);

/// Initialize the ETCA SELECT_Pseudo expansion pass.
void initializeETCASelectExpandPass(PassRegistry &);

/// Create the ETCA SELECT_Pseudo expansion pass.
FunctionPass *createETCASelectExpandPass();

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_ETCA_H
