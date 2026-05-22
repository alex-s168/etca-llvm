//===--- ETCA.h - Declare ETCA target feature support -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares ETCA TargetInfo objects.
//
// ETCA supports multiple word/pointer width combinations via -mcpu:
//   generic   → 16-bit word, 16-bit pointer (base ISA)
//   etca32    → 32-bit word, 32-bit pointer (DW + DWAS)
//   etca32p64 → 32-bit word, 64-bit pointer (DW + QWAS)
//   etca64p32 → 64-bit word, 32-bit pointer (QW + DWAS)
//   etca64    → 64-bit word, 64-bit pointer (QW + QWAS)
//
// The TargetInfo adjusts type sizes dynamically via setCPU().
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_ETCA_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_ETCA_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY ETCATargetInfo : public TargetInfo {
  // CPU kind — determines word and pointer sizes
  enum CPUKind {
    CK_Generic,   // 16b word + 16b ptr
    CK_ETCA32,    // 32b word + 32b ptr
    CK_ETCA32P64, // 32b word + 64b ptr  (32-bit ops on 64-bit regs)
    CK_ETCA64P32, // 64b word + 32b ptr  (64-bit ops, 32-bit addr)
    CK_ETCA64,    // 64b word + 64b ptr
  } CPU;

  // Derived type sizes
  unsigned WordSize = 16;
  unsigned PtrSize = 16;

  static const TargetInfo::GCCRegAlias GCCRegAliases[];
  static const char *const GCCRegNames[];

  void setWidthsFromCPU();

  /// Build the DataLayout string from the current WordSize/PtrSize.
  void updateDataLayoutString();

public:
  ETCATargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    // Default: base ISA (16-bit word, 16-bit pointer)
    CPU = CK_Generic;
    setWidthsFromCPU();
    // Don't call resetDataLayout() — Triple.computeDataLayout doesn't know about
    // ETCA. Compute the data layout string directly.
    updateDataLayoutString();
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  bool isValidCPUName(StringRef Name) const override;
  void fillValidCPUList(SmallVectorImpl<StringRef> &Values) const override;
  bool setCPU(const std::string &Name) override;

  bool hasFeature(StringRef Feature) const override;

  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override;

  std::string_view getClobbers() const override { return ""; }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::CharPtrBuiltinVaList;
  }

  bool allowsLargerPreferedTypeAlignment() const override { return false; }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_ETCA_H
