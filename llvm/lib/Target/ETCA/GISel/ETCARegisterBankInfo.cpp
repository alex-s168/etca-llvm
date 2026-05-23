//===- ETCARegisterBankInfo.cpp - ETCA Register Bank Info ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ETCARegisterBankInfo.h"
#include "ETCARegisterInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterBank.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

// Include register class IDs needed by generated bank coverage data.
#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"
#undef GET_REGINFO_ENUM

#define GET_TARGET_REGBANK_IMPL
#include "ETCAGenRegisterBank.inc"
#undef GET_TARGET_REGBANK_IMPL

#define DEBUG_TYPE "etca-regbank"

using namespace llvm;

ETCARegisterBankInfo::ETCARegisterBankInfo(const TargetRegisterInfo &TRI)
    : ETCAGenRegisterBankInfo(/*HwMode=*/0) {}

const RegisterBank &
ETCARegisterBankInfo::getRegBankFromRegClass(const TargetRegisterClass &RC,
                                             LLT Ty) const {
  // All register classes map to the single GPR bank.
  return getRegBank(ETCA::GPRBankID);
}

const RegisterBankInfo::InstructionMapping &
ETCARegisterBankInfo::getInstrMapping(const MachineInstr &MI) const {
  const unsigned Opc = MI.getOpcode();

  // Use default logic for non-generic or PHI instructions.
  // COPY is explicitly handled here because getInstrMappingImpl uses the
  // physical register's size (16 bits) for the virtual register's mapping
  // when copying from a 16-bit physical register to a wider virtual register
  // (e.g., COPY $r0 → %0:_(s32)), causing "Meaningful bits not covered"
  // assertion in RegBankSelect verification.
  if ((!isPreISelGenericOpcode(Opc) && Opc != TargetOpcode::COPY) ||
      Opc == TargetOpcode::G_PHI) {
    const InstructionMapping &Mapping = getInstrMappingImpl(MI);
    if (Mapping.isValid())
      return Mapping;
  }

  const MachineFunction &MF = *MI.getParent()->getParent();
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();

  unsigned NumOperands = MI.getNumOperands();
  if (NumOperands == 0)
    return getInvalidInstructionMapping();

  const RegisterBank &RB = getRegBank(ETCA::GPRBankID);

  // Build per-operand mappings based on each register's LLT size.
  // Different operands may have different sizes (e.g., G_ICMP: result
  // is s16, operands are s32/s64).  A single TypeSize for all operands
  // would cause "Meaningful bits not covered by the mapping" assertion
  // in RegBankSelect, because the mapping for smaller operands would
  // not cover the full width of larger ones.
  SmallVector<const ValueMapping *, 8> OpMappings;
  for (unsigned i = 0; i < NumOperands; ++i) {
    if (!MI.getOperand(i).isReg()) {
      OpMappings.push_back(nullptr);
      continue;
    }
    Register Reg = MI.getOperand(i).getReg();
    if (!Reg) {
      OpMappings.push_back(nullptr);
      continue;
    }
    // Get the size in bits for this specific register.
    TypeSize Size = getSizeInBits(Reg, MRI, TRI);
    unsigned SizeBits = Size.getKnownMinValue();
    // Ensure at least 16 bits (minimum ETCa register width).
    if (SizeBits < 16)
      SizeBits = 16;
    OpMappings.push_back(&getValueMapping(0, SizeBits, RB));
  }

  // For COPY (and other copy-like instructions), InstructionMapping::verify
  // expects exactly 1 operand in the mapping (the destination).  If we
  // included all operands, the verify check would fail because
  // isCopyLike==true implies NumOperands==1.
  //
  // Since ETCA has a single register bank (GPRBank), the dest mapping is
  // sufficient for RegBankSelect to route the copy.
  if (MI.isCopy()) {
    return getInstructionMapping(DefaultMappingID, /*Cost=*/1,
                                 getOperandsMapping(OpMappings),
                                 /*NumOperands=*/1);
  }

  return getInstructionMapping(DefaultMappingID, /*Cost=*/1,
                               getOperandsMapping(OpMappings), NumOperands);
}
