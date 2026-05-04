//===-- ETCA.h - Top-level interface for ETCA target ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Public header for the ETCA LLVM target backend. Declares the target
// registration entry point.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGET_ETCA_ETCA_H
#define LLVM_TARGET_ETCA_ETCA_H

namespace llvm {

class Target;

/// Retrieve the global ETCA target.
Target &getTheETCATarget();

} // namespace llvm

#endif // LLVM_TARGET_ETCA_ETCA_H
