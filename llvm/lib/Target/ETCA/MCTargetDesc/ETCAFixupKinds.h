//===-- ETCAFixupKinds.h - ETCA Specific Fixup Entries ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAFIXUPKINDS_H
#define LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace ETCA {

/// The set of supported fixups.
///
/// The indices in this enum MUST match the order of MCFixupKindInfo entries
/// in ETCAAsmBackend::getFixupKindInfo().
enum Fixups {
  /// No fixup.
  fixup_ETCA_NONE = FirstTargetFixupKind,

  /// 9-bit PC-relative branch displacement (R_ETCA_BASE_JMP).
  fixup_ETCA_BASE_JMP,

  /// 8-bit absolute immediate (R_ETCA_8).
  fixup_ETCA_8,

  /// 16-bit absolute immediate (R_ETCA_16).
  fixup_ETCA_16,

  /// 32-bit absolute immediate (R_ETCA_32).
  fixup_ETCA_32,

  /// 64-bit absolute immediate (R_ETCA_64).
  fixup_ETCA_64,

  /// 12-bit PC-relative SAF call displacement (R_ETCA_SAF_CALL).
  fixup_ETCA_SAF_CALL,

  // Sentinel
  NumTargetFixupKinds
};

} // namespace ETCA
} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAFIXUPKINDS_H
