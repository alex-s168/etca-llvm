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
///
/// IMPORTANT: The numerical order must match the binutils R_ETCA_* numbering
/// in include/elf/etca.h so that the fixup→relocation mapping is consistent.
enum Fixups {
  /// No fixup.
  fixup_ETCA_NONE = FirstTargetFixupKind,

  /// 9-bit PC-relative branch displacement (R_ETCA_BASE_JMP, reloc 1).
  fixup_ETCA_BASE_JMP,

  /// 8-bit absolute immediate (R_ETCA_8, reloc 49).
  fixup_ETCA_8,

  /// 16-bit absolute immediate (R_ETCA_16, reloc 50).
  fixup_ETCA_16,

  /// 32-bit absolute immediate (R_ETCA_32, reloc 51).
  fixup_ETCA_32,

  /// 64-bit absolute immediate (R_ETCA_64, reloc 52).
  fixup_ETCA_64,

  /// 12-bit PC-relative SAF call displacement (R_ETCA_SAF_CALL, reloc 6).
  fixup_ETCA_SAF_CALL,

  // --- MOV_* spanning relocations (R_ETCA_MOV_*, relocs 17-32) ---
  // These are applied to the FIRST byte of a MOVZI+SLO chain that loads
  // an absolute label address into a register.  The linker's
  // etca_build_mov_ri rewrites the entire chain in-place.
  //
  // Each fixup spans the full chain: fixup_ETCA_MOV_5 covers 1 instruction
  // (2 bytes), fixup_ETCA_MOV_10 covers 2 instructions (4 bytes), etc.
  // The mapping matches binutils R_ETCA_MOV_* numbering exactly.

  fixup_ETCA_MOV_5,  // 17: 1 insn (2B),  5-bit imm
  fixup_ETCA_MOV_10, // 18: 2 insns (4B), 10-bit imm
  fixup_ETCA_MOV_15, // 19: 3 insns (6B), 15-bit imm
  fixup_ETCA_MOV_20, // 20: 4 insns (8B), 20-bit imm
  fixup_ETCA_MOV_25, // 21: 5 insns (10B), 25-bit imm
  fixup_ETCA_MOV_30, // 22: 6 insns (12B), 30-bit imm
  fixup_ETCA_MOV_35, // 23: 7 insns (14B), 35-bit imm
  fixup_ETCA_MOV_40, // 24: 8 insns (16B), 40-bit imm
  fixup_ETCA_MOV_45, // 25: 9 insns (18B), 45-bit imm
  fixup_ETCA_MOV_50, // 26: 10 insns (20B), 50-bit imm
  fixup_ETCA_MOV_55, // 27: 11 insns (22B), 55-bit imm
  fixup_ETCA_MOV_60, // 28: 12 insns (24B), 60-bit imm
  fixup_ETCA_MOV_64, // 29: 13 insns (26B), 64-bit imm
  fixup_ETCA_MOV_8,  // 30: 2 insns (4B), 8-bit imm (byte, special)
  fixup_ETCA_MOV_16, // 31: 4 insns (8B), 16-bit imm
  fixup_ETCA_MOV_32, // 32: 7 insns (14B), 32-bit imm

  // Sentinel
  NumTargetFixupKinds
};

/// Return the MOV_* fixup kind for a given pointer width (in bits).
/// Returns fixup_ETCA_NONE for unsupported widths.
inline Fixups getMovFixupForPtrSize(unsigned PtrBits) {
  if (PtrBits >= 64)
    return fixup_ETCA_MOV_64;
  if (PtrBits >= 32)
    return fixup_ETCA_MOV_32;
  if (PtrBits >= 16)
    return fixup_ETCA_MOV_16;
  return fixup_ETCA_NONE;
}

/// Return the byte count of the MOV_* chain for a given fixup kind.
inline unsigned getMovChainBytes(Fixups Fixup) {
  switch (Fixup) {
  case fixup_ETCA_MOV_5:
    return 2;
  case fixup_ETCA_MOV_8:
    return 4;
  case fixup_ETCA_MOV_10:
    return 4;
  case fixup_ETCA_MOV_15:
    return 6;
  case fixup_ETCA_MOV_16:
    return 8;
  case fixup_ETCA_MOV_20:
    return 8;
  case fixup_ETCA_MOV_25:
    return 10;
  case fixup_ETCA_MOV_30:
    return 12;
  case fixup_ETCA_MOV_32:
    return 14;
  case fixup_ETCA_MOV_35:
    return 14;
  case fixup_ETCA_MOV_40:
    return 16;
  case fixup_ETCA_MOV_45:
    return 18;
  case fixup_ETCA_MOV_50:
    return 20;
  case fixup_ETCA_MOV_55:
    return 22;
  case fixup_ETCA_MOV_60:
    return 24;
  case fixup_ETCA_MOV_64:
    return 26;
  default:
    return 0;
  }
}

} // namespace ETCA
} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_MCTARGETDESC_ETCAFIXUPKINDS_H
