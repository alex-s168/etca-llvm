//===-- ETCAMCAsmInfo.h - ETCA Asm Info ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the ETCAMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAMCASMINFO_H
#define LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {

class Triple;

class ETCAMCAsmInfo : public MCAsmInfoELF {
public:
  explicit ETCAMCAsmInfo(const Triple &TT, const MCTargetOptions &Options,
                         unsigned PtrSize = 16);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAMCASMINFO_H
