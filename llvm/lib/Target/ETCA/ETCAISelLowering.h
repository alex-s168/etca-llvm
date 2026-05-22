//===- ETCAISelLowering.h - ETCA DAG Lowering Interface ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ETCA target lowering configuration.
//
// This file defines the ETCATargetLowering class, which configures which
// operations are legal/expand/custom for the ETCA target. This is used by
// the GlobalISel pipeline (via getTargetLowering()).
//
// NOTE: SDAG lowering hooks (LowerOperation, LowerFormalArguments, etc.)
// have been deliberately removed. ETCA uses GlobalISel exclusively.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_ETCASELLOWERING_H
#define LLVM_LIB_TARGET_ETCA_ETCASELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class ETCASubtarget;
class SelectionDAG;
class SDValue;

class ETCATargetLowering : public TargetLowering {
public:
  explicit ETCATargetLowering(const TargetMachine &TM,
                              const ETCASubtarget &STI);

  /// Return the ETCASubtarget object.
  const ETCASubtarget &getSubtarget() const { return STI; }

  /// Return true if the immediate can be used directly in a CMPI instruction.
  /// ETCa RI format uses a 5-bit sign-extended immediate ([-16, 15]).
  bool isLegalICmpImmediate(int64_t Imm) const override {
    return isInt<5>(Imm);
  }

  /// Return true if the immediate can be used directly in an ADDI instruction.
  /// ETCa RI format uses a 5-bit sign-extended immediate ([-16, 15]).
  bool isLegalAddImmediate(int64_t Imm) const override { return isInt<5>(Imm); }

  /// Return true if the addressing mode is legal for ETCA.
  bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM, Type *Ty,
                             unsigned AS,
                             Instruction *I = nullptr) const override;

private:
  const ETCASubtarget &STI;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_ETCASELLOWERING_H
