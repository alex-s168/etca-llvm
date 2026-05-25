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
class LegalizerHelper;

/// This class provides the legalization rules for the ETCA target.
class ETCALegalizerInfo : public LegalizerInfo {
public:
  ETCALegalizerInfo(const ETCASubtarget &ST);

  bool legalizeCustom(LegalizerHelper &Helper, MachineInstr &MI,
                      LostDebugLocObserver &LocObserver) const override;

private:
  /// Minimum legal scalar size in bits (8 with byte ext, 16 otherwise).
  unsigned MinLegalSize;

  /// Expand G_BRJT into load + G_BRINDIRECT.
  bool legalizeBRJT(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Widen a sub-MinLegal G_LOAD to the minimum legal scalar type,
  /// then truncate to the original type.
  bool legalizeSubMinLegalLoad(MachineInstr &MI,
                               MachineIRBuilder &MIRBuilder) const;

  /// Widen a sub-MinLegal G_STORE to the minimum legal scalar type
  /// by extending the value before storing.
  bool legalizeSubMinLegalStore(MachineInstr &MI,
                                MachineIRBuilder &MIRBuilder) const;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_GISEL_ETCAGLOBALISEL_H
