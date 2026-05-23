//===-- ETCAMCTargetDesc.h - ETCA Target Descriptions ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides ETCA specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAMCTARGETDESC_H
#define LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAMCTARGETDESC_H

#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class Target;

MCCodeEmitter *createETCAMCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx);

MCAsmBackend *createETCAAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                   const MCRegisterInfo &MRI,
                                   const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createETCAELFObjectWriter(uint8_t OSABI,
                                                                bool Is64Bit);

} // namespace llvm

// Pull in register and subtarget enums for use by other files.
#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "ETCAGenSubtargetInfo.inc"

#endif // LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAMCTARGETDESC_H
