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
// ETCA supports multiple word/pointer width combinations via -mattr
// feature flags rather than separate CPU models.  The only valid CPU
// name is "generic".  Select sizes via:
//   32-bit word + 32-bit ptr:  -mattr=+32bit,+ptr32,+dw
//   64-bit word + 64-bit ptr:  -mattr=+64bit,+ptr64,+dw,+qw
//   64-bit word + 32-bit ptr:  -mattr=+64bit,+ptr32,+dw,+qw
//   16-bit word + 16-bit ptr:  (default, no extra flags needed)
//
// The TargetInfo adjusts type sizes dynamically via handleTargetFeatures().
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
  // Derived type sizes — set from -mattr feature flags via
  // handleTargetFeatures().
  unsigned WordSize = 16;
  unsigned PtrSize = 16;

  static const TargetInfo::GCCRegAlias GCCRegAliases[];
  static const char *const GCCRegNames[];

  // REX extension availability
  bool HasREX = false;

  void setWidthsFromFeatures();

  /// Build the DataLayout string from the current WordSize/PtrSize.
  void updateDataLayoutString();

public:
  ETCATargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    // Default: base ISA (16-bit word, 16-bit pointer).
    // WordSize/PtrSize may be updated by handleTargetFeatures() when
    // -mattr features are processed.
    setWidthsFromFeatures();
    // Don't call resetDataLayout() — Triple.computeDataLayout doesn't know about
    // ETCA. Compute the data layout string directly.
    updateDataLayoutString();
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  bool isValidCPUName(StringRef Name) const override;
  void fillValidCPUList(SmallVectorImpl<StringRef> &Values) const override;
  bool setCPU(const std::string &Name) override;

  bool handleTargetFeatures(std::vector<std::string> &Features,
                            DiagnosticsEngine &Diags) override;

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
