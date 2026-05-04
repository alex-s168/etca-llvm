//===- ETCARegisterBankInfo.h - ETCA Register Bank Info --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_GISEL_ETCAREGISTERBANKINFO_H
#define LLVM_LIB_TARGET_ETCA_GISEL_ETCAREGISTERBANKINFO_H

#include "llvm/CodeGen/RegisterBankInfo.h"

#define GET_REGBANK_DECLARATIONS
#include "ETCAGenRegisterBank.inc"
#undef GET_REGBANK_DECLARATIONS

namespace llvm {

class TargetRegisterInfo;

/// Generated register bank info base class for ETCA.
class ETCAGenRegisterBankInfo : public RegisterBankInfo {
#define GET_TARGET_REGBANK_CLASS
#include "ETCAGenRegisterBank.inc"
#undef GET_TARGET_REGBANK_CLASS
};

/// ETCA register bank info.
class ETCARegisterBankInfo final : public ETCAGenRegisterBankInfo {
public:
  ETCARegisterBankInfo(const TargetRegisterInfo &TRI);

  const RegisterBank &
  getRegBankFromRegClass(const TargetRegisterClass &RC,
                         LLT Ty) const override;

  const InstructionMapping &
  getInstrMapping(const MachineInstr &MI) const override;
};

} // namespace llvm

#endif
