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

  /// Lower G_UADDSAT via MinMax expansion: a + umin(~a, b).
  bool legalizeUAddSat(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Lower G_USUBSAT via MinMax expansion: a - umin(a, b).
  bool legalizeUSubSat(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Lower G_SADDSAT via MinMax expansion.
  bool legalizeSAddSat(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Lower G_SSUBSAT via MinMax expansion.
  bool legalizeSSubSat(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Expand G_UADDO to G_ADD + G_ICMP.
  bool legalizeUAddo(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Expand G_USUBO to G_SUB + G_ICMP.
  bool legalizeUSubo(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Expand G_UADDE to G_ADD chain + G_ICMP + G_OR.
  bool legalizeUAdde(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Expand G_USUBE to G_SUB chain + G_ICMP + G_OR.
  bool legalizeUSube(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Legalize G_VASTART: store the address of the varargs save area
  /// into the va_list pointer.
  bool legalizeVAStart(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  //===---------------------------------------------------------------===//
  // Soft-float custom legalization helpers
  //===---------------------------------------------------------------===//

  /// Lower G_FNEG to XOR with sign bit mask.
  bool legalizeFNEG(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Lower G_FABS to AND with sign bit cleared.
  bool legalizeFABS(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Bitcast G_FCONSTANT to G_CONSTANT (FP constant materialized as
  /// integer bit pattern).
  bool legalizeFCONSTANT(MachineInstr &MI, MachineIRBuilder &MIRBuilder) const;

  /// Emit a call to a named soft-float library function.
  /// Picks Name32 (e.g. "__sqrtf") for 32-bit FP or Name64 (e.g. "__sqrt")
  /// for 64-bit FP.  Handles unary, binary, and ternary FP ops.
  bool legalizeFPLibcall(MachineInstr &MI, MachineIRBuilder &MIRBuilder,
                         const char *Name32, const char *Name64) const;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_GISEL_ETCAGLOBALISEL_H
