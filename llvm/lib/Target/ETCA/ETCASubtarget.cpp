//===-- ETCASubtarget.cpp - ETCA Subtarget Information -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the ETCA specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "ETCASubtarget.h"
#include "ETCAISelLowering.h"
#include "ETCAInstrInfo.h"
#include "ETCARegisterInfo.h"
#include "GISel/ETCAInstructionSelector.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "etca-subtarget"

#define GET_SUBTARGETINFO_ENUM
#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "ETCAGenSubtargetInfo.inc"

std::string ETCASubtarget::buildDataLayoutString(unsigned WordSize,
                                                 unsigned PtrSize) {
  // Format: "e-m:e-p:PS:PS-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-a:0-n8:16"
  // where PS is the pointer size.
  // For 16-bit word size, i32/i64/f32/f64 are not natively aligned (they are
  // 16-bit). For 32-bit word size, i64/f64 are not natively aligned. For 64-bit
  // word size, everything is natively aligned.
  //
  // Key: i32:WS means i32 is WS-bit aligned in memory (where WS = WordSize).
  // The pointer size determines p:PS:PS.

  std::string Ret;
  raw_string_ostream OS(Ret);

  OS << "e-m:e"                             // little-endian, ELF mangling
     << "-p:" << PtrSize << ":" << PtrSize; // pointer size & ABI alignment

  // Integer alignments: i8 always byte-aligned, i16 at 16-bit aligns,
  // i32/i64 align to min(word_size, size).
  OS << "-i8:8";
  if (WordSize >= 16)
    OS << "-i16:16";
  else
    OS << "-i16:8";

  if (WordSize >= 32)
    OS << "-i32:32";
  else
    OS << "-i32:16"; // i32 is 16-bit aligned on 16-bit machines

  if (WordSize >= 64) {
    OS << "-i64:64";
    OS << "-i128:128"; // i128 naturally aligned on 64-bit machines
  } else if (WordSize >= 32)
    OS << "-i64:32"; // i64 is 32-bit aligned on 32-bit machines
  else
    OS << "-i64:16"; // i64 is 16-bit aligned on 16-bit machines

  // Float/double alignments follow the same logic as integers:
  // aligned to min(word_size, type_size).
  if (WordSize >= 32)
    OS << "-f32:32";
  else
    OS << "-f32:16"; // f32 is 16-bit aligned on 16-bit machines

  if (WordSize >= 64)
    OS << "-f64:64";
  else if (WordSize >= 32)
    OS << "-f64:32"; // f64 is 32-bit aligned on 32-bit machines
  else
    OS << "-f64:16"; // f64 is 16-bit aligned on 16-bit machines

  OS << "-a:0"    // aggregate alignment 0 = use natural alignment
     << "-n8:16"; // native integer widths: 8, 16 bits
  if (WordSize >= 32)
    OS << ":32"; // 32-bit native on 32/64-bit word machines
  if (WordSize >= 64)
    OS << ":64";          // 64-bit native on 64-bit word machines
  OS << "-S" << WordSize; // stack alignment = register width (in bits)

  return Ret;
}

void ETCASubtarget::buildDLString() {
  DLString = buildDataLayoutString(WordSize, PtrSize);
}

void ETCASubtarget::initLibcallLoweringInfo(LibcallLoweringInfo &Info) const {
  // Register standard compiler-rt libcall implementations for integer
  // MUL, DIV, and REM operations that are not natively supported.
  //
  // The default RuntimeLibcallsInfo does not set ANY implementations as
  // available for unrecognized target triples (like ETCA's custom triple).
  // Each target must register its own available implementations.

  // Integer arithmetic and shift libcalls
  const struct {
    const RTLIB::Libcall Op;
    const RTLIB::LibcallImpl Impl;
  } IntLibcalls[] = {
      {RTLIB::MUL_I8, RTLIB::impl___mulqi3},
      {RTLIB::MUL_I16, RTLIB::impl___mulhi3},
      {RTLIB::MUL_I32, RTLIB::impl___mulsi3},
      {RTLIB::MUL_I64, RTLIB::impl___muldi3},
      {RTLIB::SDIV_I8, RTLIB::impl___divqi3},
      {RTLIB::SDIV_I16, RTLIB::impl___divhi3},
      {RTLIB::SDIV_I32, RTLIB::impl___divsi3},
      {RTLIB::SDIV_I64, RTLIB::impl___divdi3},
      {RTLIB::UDIV_I8, RTLIB::impl___udivqi3},
      {RTLIB::UDIV_I16, RTLIB::impl___udivhi3},
      {RTLIB::UDIV_I32, RTLIB::impl___udivsi3},
      {RTLIB::UDIV_I64, RTLIB::impl___udivdi3},
      {RTLIB::SREM_I8, RTLIB::impl___modqi3},
      {RTLIB::SREM_I16, RTLIB::impl___modhi3},
      {RTLIB::SREM_I32, RTLIB::impl___modsi3},
      {RTLIB::SREM_I64, RTLIB::impl___moddi3},
      {RTLIB::UREM_I8, RTLIB::impl___umodqi3},
      {RTLIB::UREM_I16, RTLIB::impl___umodhi3},
      {RTLIB::UREM_I32, RTLIB::impl___umodsi3},
      {RTLIB::UREM_I64, RTLIB::impl___umoddi3},
      // Integer shift-right libcalls
      {RTLIB::SRA_I16, RTLIB::impl___ashrhi3},
      {RTLIB::SRA_I32, RTLIB::impl___ashrsi3},
      {RTLIB::SRA_I64, RTLIB::impl___ashrdi3},
      {RTLIB::SRL_I16, RTLIB::impl___lshrhi3},
      {RTLIB::SRL_I32, RTLIB::impl___lshrsi3},
      {RTLIB::SRL_I64, RTLIB::impl___lshrdi3},
      // Integer shift-left libcalls
      {RTLIB::SHL_I16, RTLIB::impl___ashlhi3},
      {RTLIB::SHL_I32, RTLIB::impl___ashlsi3},
      {RTLIB::SHL_I64, RTLIB::impl___ashldi3},
  };
  for (const auto &LC : IntLibcalls)
    Info.setLibcallImpl(LC.Op, LC.Impl);

  // Memory intrinsics — memcpy, memmove, memset
  // The default RuntimeLibcallsInfo is empty for unknown triples (like
  // ETCA's custom triple), so we must register these explicitly.
  // The implementations map to the standard C library functions which
  // must be provided by the runtime environment.
  const struct {
    const RTLIB::Libcall Op;
    const RTLIB::LibcallImpl Impl;
  } MemLibcalls[] = {
      {RTLIB::MEMCPY, RTLIB::impl_memcpy},
      {RTLIB::MEMMOVE, RTLIB::impl_memmove},
      {RTLIB::MEMSET, RTLIB::impl_memset},
  };
  for (const auto &LC : MemLibcalls)
    Info.setLibcallImpl(LC.Op, LC.Impl);
}

ETCASubtarget::ETCASubtarget(const Triple &TargetTriple, StringRef Cpu,
                             StringRef FeatureString, const TargetMachine &TM,
                             const TargetOptions &Options,
                             std::optional<CodeModel::Model> CodeModel,
                             std::optional<CodeGenOptLevel> OptLevel)
    : ETCAGenSubtargetInfo(TargetTriple, Cpu, Cpu, FeatureString), TM(TM) {
  ParseSubtargetFeatures(Cpu, Cpu, FeatureString);
  // Now WordSize is known; construct FrameLowering with correct alignment.
  FrameLowering = std::make_unique<ETCAFrameLowering>(Align(WordSize / 8));
  buildDLString();
  initSubtargetDeps();
}

ETCASubtarget::~ETCASubtarget() = default;

void ETCASubtarget::initSubtargetDeps() const {
  auto *ST = const_cast<ETCASubtarget *>(this);
  ST->RegInfo = std::make_unique<ETCARegisterInfo>(*this);
  ST->InstrInfo = std::make_unique<ETCAInstrInfo>(*ST, *ST->RegInfo);
  ST->TLInfo = std::make_unique<ETCATargetLowering>(this->TM, *this);
}

const TargetInstrInfo *ETCASubtarget::getInstrInfo() const {
  return InstrInfo.get();
}

const TargetRegisterInfo *ETCASubtarget::getRegisterInfo() const {
  return RegInfo.get();
}

const TargetLowering *ETCASubtarget::getTargetLowering() const {
  return TLInfo.get();
}

const CallLowering *ETCASubtarget::getCallLowering() const {
  if (!CallLoweringInfo)
    CallLoweringInfo = std::make_unique<ETCACallLowering>(getTargetLowering());
  return CallLoweringInfo.get();
}

const LegalizerInfo *ETCASubtarget::getLegalizerInfo() const {
  if (!Legalizer)
    Legalizer = std::make_unique<ETCALegalizerInfo>(*this);
  return Legalizer.get();
}

const RegisterBankInfo *ETCASubtarget::getRegBankInfo() const {
  if (!RegBankInfo)
    RegBankInfo = std::make_unique<ETCARegisterBankInfo>(*getRegisterInfo());
  return RegBankInfo.get();
}

InstructionSelector *ETCASubtarget::getInstructionSelector() const {
  if (!InstSelector)
    InstSelector =
        std::make_unique<ETCAInstructionSelector>(*this, *getRegBankInfo());
  return InstSelector.get();
}

const TargetRegisterClass *ETCASubtarget::getGPRRegClass() const {
  return getGPRRegClassForWidth(WordSize);
}

const TargetRegisterClass *ETCASubtarget::getBaseOrRexGPRRegClass() const {
  // Returns the REX-extended class when HasREX, else the base class.
  if (HasREX)
    return getGPRRegClassForWidth(WordSize);
  return getGPRRegClass();
}

const TargetRegisterClass *
ETCASubtarget::getGPRRegClassForWidth(unsigned Width) const {
  // Always return the full 16-register class since GPR/GPR32/GPR64 now
  // contain all 16 registers.  The HasREX flag controls register allocation
  // in the legalizer (hasREX() gates which operations can use high regs).
  switch (Width) {
  case 32:
    return &ETCA::GPR32RegClass;
  case 64:
    return &ETCA::GPR64RegClass;
  default:
    return &ETCA::GPRRegClass;
  }
}
// DEBUG
#include "llvm/Support/raw_ostream.h"
