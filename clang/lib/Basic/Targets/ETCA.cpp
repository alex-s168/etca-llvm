//===--- ETCA.cpp - Implement ETCA target feature support -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements ETCA TargetInfo objects.
//
// ETCA is a custom 16-bit RISC ISA with variable word/pointer widths:
//   generic   → 16b word + 16b ptr (base ISA + SAF)
//   etca32    → 32b word + 32b ptr (DW + DWAS)
//   etca32p64 → 32b word + 64b ptr (DW + QWAS)
//   etca64p32 → 64b word + 32b ptr (QW + DWAS)
//   etca64    → 64b word + 64b ptr (QW + QWAS)
//===----------------------------------------------------------------------===//

#include "ETCA.h"
#include "clang/Basic/MacroBuilder.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::targets;

//===----------------------------------------------------------------------===//
// ETCA register names for inline assembly
//===----------------------------------------------------------------------===//

const char *const ETCATargetInfo::GCCRegNames[] = {
    "r0", "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "d0", "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7",
    "q0", "q1",  "q2",  "q3",  "q4",  "q5",  "q6",  "q7",
};

ArrayRef<const char *> ETCATargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

const TargetInfo::GCCRegAlias ETCATargetInfo::GCCRegAliases[] = {
    // ABI argument registers
    {{"a0"}, "r0"},  {{"a0d"}, "d0"}, {{"a0q"}, "q0"},
    {{"a1"}, "r1"},  {{"a1d"}, "d1"}, {{"a1q"}, "q1"},
    {{"a2"}, "r2"},  {{"a2d"}, "d2"}, {{"a2q"}, "q2"},

    // ABI callee-saved registers
    {{"s0"}, "r3"},  {{"s0d"}, "d3"}, {{"s0q"}, "q3"},
    {{"s1"}, "r4"},  {{"s1d"}, "d4"}, {{"s1q"}, "q4"},

    // Special registers
    {{"bp"}, "r5"},  {{"bpd"}, "d5"}, {{"bpq"}, "q5"},
    {{"sp"}, "r6"},  {{"spd"}, "d6"}, {{"spq"}, "q6"},
    {{"ln"}, "r7"},  {{"lnd"}, "d7"}, {{"lnq"}, "q7"},

    // Backward-compatible names
    {{"r0x"}, "r0"}, {{"r0h"}, "r0"},
    {{"r1x"}, "r1"}, {{"r1h"}, "r1"},
    {{"r2x"}, "r2"}, {{"r2h"}, "r2"},
    {{"r3x"}, "r3"}, {{"r3h"}, "r3"},
    {{"r4x"}, "r4"}, {{"r4h"}, "r4"},
    {{"r5x"}, "r5"}, {{"r5h"}, "r5"},
    {{"r6x"}, "r6"}, {{"r6h"}, "r6"},
    {{"r7x"}, "r7"}, {{"r7h"}, "r7"},
};

ArrayRef<TargetInfo::GCCRegAlias> ETCATargetInfo::getGCCRegAliases() const {
  return llvm::ArrayRef(GCCRegAliases);
}

//===----------------------------------------------------------------------===//
// CPU name validation
//===----------------------------------------------------------------------===//

bool ETCATargetInfo::isValidCPUName(StringRef Name) const {
  return llvm::StringSwitch<bool>(Name)
      .Case("generic", true)
      .Case("etca32", true)
      .Case("etca32p64", true)
      .Case("etca64p32", true)
      .Case("etca64", true)
      .Default(false);
}

void ETCATargetInfo::fillValidCPUList(SmallVectorImpl<StringRef> &Values) const {
  Values.emplace_back("generic");
  Values.emplace_back("etca32");
  Values.emplace_back("etca32p64");
  Values.emplace_back("etca64p32");
  Values.emplace_back("etca64");
}

void ETCATargetInfo::setWidthsFromCPU() {
  switch (CPU) {
  case CK_Generic:
    WordSize = 16;
    PtrSize = 16;
    break;
  case CK_ETCA32:
    WordSize = 32;
    PtrSize = 32;
    break;
  case CK_ETCA32P64:
    WordSize = 32;
    PtrSize = 64;
    break;
  case CK_ETCA64P32:
    WordSize = 64;
    PtrSize = 32;
    break;
  case CK_ETCA64:
    WordSize = 64;
    PtrSize = 64;
    break;
  }

  // Set C type sizes based on word and pointer widths.
  //
  // ETCA uses a LP-like model:
  //   16-bit word: int = 16 bits, long = 32 bits
  //   32-bit word: int = 32 bits, long = 32 bits (ILP32-like)
  //   64-bit word: int = 32 bits, long = 64 bits (LP64-like)
  //
  // LongLong is always 64 bits.  Float is 32, Double is 64.
  // Pointer size follows the address width (PtrSize).

  // --- Integer types ---
  IntWidth = (WordSize >= 32) ? 32 : 16;
  IntAlign = IntWidth;

  LongWidth = (WordSize >= 64) ? 64 : 32;
  LongAlign = LongWidth;

  LongLongWidth = 64;
  LongLongAlign = (WordSize >= 64) ? 64 : (WordSize >= 32 ? 32 : 16);

  // --- Floating-point types ---
  FloatWidth = 32;
  FloatAlign = (WordSize >= 32) ? 32 : 16;

  DoubleWidth = 64;
  DoubleAlign = (WordSize >= 64) ? 64 : (WordSize >= 32 ? 32 : 16);

  LongDoubleWidth = 64;
  LongDoubleAlign = (WordSize >= 64) ? 64 : (WordSize >= 32 ? 32 : 16);

  // --- Pointer ---
  PointerWidth = PtrSize;
  PointerAlign = PtrSize;

  // --- Aggregate alignment ---
  SuitableAlign = (WordSize >= 64) ? 64 : 32;

  // --- Derived type mappings ---
  if (PtrSize == 64) {
    SizeType = UnsignedLong;
    IntMaxType = SignedLongLong;
    IntPtrType = SignedLong;
    PtrDiffType = SignedLong;
    SigAtomicType = SignedLongLong;
  } else if (PtrSize == 32) {
    SizeType = UnsignedInt;
    IntMaxType = SignedLongLong;
    IntPtrType = SignedInt;
    PtrDiffType = SignedInt;
    SigAtomicType = SignedLong;
  } else {
    // 16-bit pointer
    SizeType = UnsignedInt;
    IntMaxType = SignedLongLong;
    IntPtrType = SignedInt;
    PtrDiffType = SignedInt;
    SigAtomicType = SignedLong;
  }

  // No TLS for bare-metal target
  TLSSupported = false;
}

bool ETCATargetInfo::setCPU(const std::string &Name) {
  CPU = llvm::StringSwitch<CPUKind>(Name)
            .Case("generic", CK_Generic)
            .Case("etca32", CK_ETCA32)
            .Case("etca32p64", CK_ETCA32P64)
            .Case("etca64p32", CK_ETCA64P32)
            .Case("etca64", CK_ETCA64)
            .Default(CK_Generic);

  setWidthsFromCPU();

  // Recompute the data layout string for this CPU variant.
  std::string DL;
  llvm::raw_string_ostream OS(DL);

  OS << "e-m:e"                                // little-endian, ELF mangling
     << "-p:" << PtrSize << ":" << PtrSize;    // pointer: size, ABI align

  OS << "-i8:8"
     << "-i16:16"
     << "-i32:" << (WordSize >= 32 ? 32 : 16)
     << "-i64:" << (WordSize >= 64 ? 64 : (WordSize >= 32 ? 32 : 16));

  OS << "-a:0"                                 // aggregate: natural alignment
     << "-n8:16"                               // native integer widths
     << "-S" << WordSize;                      // stack alignment in bits

  DataLayoutString = DL;

  return true;
}

//===----------------------------------------------------------------------===//
// Feature detection
//===----------------------------------------------------------------------===//

bool ETCATargetInfo::hasFeature(StringRef Feature) const {
  return llvm::StringSwitch<bool>(Feature)
      .Case("etca", true)
      .Case("no-saf", false)
      .Case("saf", true)          // SAF is always available on all CPUs
      .Case("byte", true)         // BYTE extension is available (SS=00 ops)
      .Case("16bit", true)        // 16-bit ops always available
      .Case("32bit", WordSize >= 32)
      .Case("64bit", WordSize >= 64)
      .Case("dw", WordSize >= 32)
      .Case("qw", WordSize >= 64)
      .Case("ptr16", PtrSize == 16)
      .Case("ptr32", PtrSize == 32)
      .Case("ptr64", PtrSize == 64)
      .Case("dwas", PtrSize >= 32)
      .Case("qwas", PtrSize >= 64)
      .Default(false);
}

//===----------------------------------------------------------------------===//
// Target defines
//===----------------------------------------------------------------------===//

void ETCATargetInfo::getTargetDefines(const LangOptions &Opts,
                                      MacroBuilder &Builder) const {
  // Machine-level macros
  Builder.defineMacro("__etca__");
  Builder.defineMacro("__ETCA__");
  Builder.defineMacro("__ELF__");

  // Word size
  Builder.defineMacro("__ETCA_WORD_SIZE__", Twine(WordSize));

  // Pointer size
  Builder.defineMacro("__SIZEOF_POINTER__", Twine(PtrSize / 8));
  Builder.defineMacro("__ETCA_PTR_SIZE__", Twine(PtrSize));

  // CPU-specific macros
  switch (CPU) {
  case CK_Generic:
    Builder.defineMacro("__ETCA_GENERIC__");
    break;
  case CK_ETCA32:
    Builder.defineMacro("__ETCA32__");
    break;
  case CK_ETCA32P64:
    Builder.defineMacro("__ETCA32P64__");
    break;
  case CK_ETCA64P32:
    Builder.defineMacro("__ETCA64P32__");
    break;
  case CK_ETCA64:
    Builder.defineMacro("__ETCA64__");
    break;
  }

  // Extension macros
  if (WordSize >= 32)
    Builder.defineMacro("__ETCA_HAS_DW__");
  if (WordSize >= 64)
    Builder.defineMacro("__ETCA_HAS_QW__");
  if (PtrSize >= 32)
    Builder.defineMacro("__ETCA_HAS_DWAS__");
  if (PtrSize >= 64)
    Builder.defineMacro("__ETCA_HAS_QWAS__");

  // SAF is always available on all supported CPUs
  Builder.defineMacro("__ETCA_HAS_SAF__");

  // BYTE extension always available
  Builder.defineMacro("__ETCA_HAS_BYTE__");
}

//===----------------------------------------------------------------------===//
// Inline assembly constraint validation
//===----------------------------------------------------------------------===//

bool ETCATargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  switch (*Name) {
  case 'r': // General purpose register
    Info.setAllowsRegister();
    return true;
  case 'i': // Immediate constant
    Info.setRequiresImmediate();
    return true;
  case 'm': // Memory operand
    return true;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7': // Specific register constraint (r0-r7 / d0-d7 / q0-q7)
    Info.setAllowsRegister();
    return true;
  default:
    return false;
  }
}
