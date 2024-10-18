//===-- EtcaRegisterInfo.cpp - Etca Register Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Etca implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "EtcaRegisterInfo.h"
#include "EtcaFrameLowering.h"
#include "EtcaInstrInfo.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_REGINFO_TARGET_DESC
#include "EtcaGenRegisterInfo.inc"

using namespace llvm;

EtcaRegisterInfo::EtcaRegisterInfo() : EtcaGenRegisterInfo(Etca::RCA) {}

const uint16_t *
EtcaRegisterInfo::getCalleeSavedRegs(const MachineFunction * /*MF*/) const {
  return CSR_SaveList;
}

BitVector EtcaRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  if (hasBasePointer(MF))
    Reserved.set(getBaseRegister());
  return Reserved;
}

bool EtcaRegisterInfo::requiresRegisterScavenging(
    const MachineFunction & /*MF*/) const {
  return true;
}

bool EtcaRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *RS) const {
  return false;
}

bool EtcaRegisterInfo::hasBasePointer(const MachineFunction &MF) const {
  return true;
}

unsigned EtcaRegisterInfo::getRARegister() const { return Etca::GPR; }

Register
EtcaRegisterInfo::getFrameRegister(const MachineFunction & /*MF*/) const {
  return Etca::BP;
}

Register EtcaRegisterInfo::getBaseRegister() const { return Etca::BP; }

const uint32_t *
EtcaRegisterInfo::getCallPreservedMask(const MachineFunction & /*MF*/,
                                        CallingConv::ID /*CC*/) const {
  return CSR_RegMask;
}
