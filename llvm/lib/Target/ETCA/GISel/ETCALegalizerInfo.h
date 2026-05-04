//===- ETCALegalizerInfo.h - ETCA Legalizer -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the ETCA-specific legalizer info for GlobalISel.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_GISEL_ETCAGLOBALISEL_H
#define LLVM_LIB_TARGET_ETCA_GISEL_ETCAGLOBALISEL_H

#include "llvm/CodeGen/GlobalISel/LegalizerInfo.h"

namespace llvm {

class ETCASubtarget;

/// This class provides the legalization rules for the ETCA target.
class ETCALegalizerInfo : public LegalizerInfo {
public:
  ETCALegalizerInfo(const ETCASubtarget &ST);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_GISEL_ETCAGLOBALISEL_H
