//=====-- EtcaMCAsmInfo.h - Etca asm properties -----------*- C++ -*--====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the EtcaMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAMCASMINFO_H
#define LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class EtcaMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit EtcaMCAsmInfo(const Triple &TheTriple,
                          const MCTargetOptions &Options);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAMCASMINFO_H
