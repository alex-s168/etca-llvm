//===-- ETCASubtarget.h - Define Subtarget for the ETCA -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the ETCA specific subclass of TargetSubtarget.
//
// ETCA supports multiple word/pointer width combinations via subtarget
// features:
//   - WordSize (16, 32, 64): physical register width
//   - PtrSize  (16, 32, 64): address/pointer width
//   - Extensions: SAF, DW, QW, DWAS, QWAS
//
// The DataLayout is constructed dynamically based on these features.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_ETCASUBTARGET_H
#define LLVM_LIB_TARGET_ETCA_ETCASUBTARGET_H

#include "ETCAFrameLowering.h"
#include "GISel/ETCACallLowering.h"
#include "GISel/ETCALegalizerInfo.h"
#include "GISel/ETCARegisterBankInfo.h"
#include "llvm/CodeGen/GlobalISel/CallLowering.h"
#include "llvm/CodeGen/GlobalISel/LegalizerInfo.h"
#include "llvm/CodeGen/LibcallLoweringInfo.h"
#include "llvm/CodeGen/RegisterBankInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"
#include <string>

#define GET_SUBTARGETINFO_HEADER
#include "ETCAGenSubtargetInfo.inc"

namespace llvm {

class ETCAInstrInfo;
struct ETCARegisterInfo;
class TargetLowering;

class ETCASubtarget : public ETCAGenSubtargetInfo {
  const TargetMachine &TM;

  // Word size (16, 32, or 64) — determines physical register width
  unsigned WordSize = 16;

  // Pointer size (16, 32, or 64) — determines address register width
  unsigned PtrSize = 16;

  // Extension flags
  bool HasSAF = false;
  bool HasByte = false;
  bool HasREX = false;
  bool HasDW = false;
  bool HasQW = false;
  bool HasDWAS = false;
  bool HasQWAS = false;

  // Target objects owned by the subtarget
  // FrameLowering is a pointer because the stack alignment depends on
  // WordSize which is only known after ParseSubtargetFeatures runs.
  std::unique_ptr<ETCAFrameLowering> FrameLowering;
  std::unique_ptr<ETCARegisterInfo> RegInfo;
  std::unique_ptr<ETCAInstrInfo> InstrInfo;
  std::unique_ptr<TargetLowering> TLInfo;

  // GISel components
  mutable std::unique_ptr<ETCALegalizerInfo> Legalizer;
  mutable std::unique_ptr<ETCARegisterBankInfo> RegBankInfo;
  mutable std::unique_ptr<ETCACallLowering> CallLoweringInfo;
  mutable std::unique_ptr<InstructionSelector> InstSelector;

  // Dynamic data layout string
  std::string DLString;

  void initSubtargetDeps(const TargetMachine &TM) const;

public:
  ETCASubtarget(const Triple &TargetTriple, StringRef Cpu,
                StringRef FeatureString, const TargetMachine &TM,
                const TargetOptions &Options,
                std::optional<CodeModel::Model> CodeModel,
                std::optional<CodeGenOptLevel> OptLevel);

  ~ETCASubtarget() override;

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  const TargetFrameLowering *getFrameLowering() const override {
    return FrameLowering.get();
  }
  const TargetInstrInfo *getInstrInfo() const override;
  const TargetRegisterInfo *getRegisterInfo() const override;
  const TargetLowering *getTargetLowering() const override;

  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return nullptr;
  }

  // GISel accessors
  const CallLowering *getCallLowering() const override;
  const LegalizerInfo *getLegalizerInfo() const override;
  const RegisterBankInfo *getRegBankInfo() const override;
  InstructionSelector *getInstructionSelector() const override;

  // Width accessors
  unsigned getWordSize() const { return WordSize; }
  unsigned getPtrSize() const { return PtrSize; }

  // Extension accessors
  bool hasSAF() const { return HasSAF; }
  bool hasByte() const { return HasByte; }
  bool hasREX() const { return HasREX; }
  bool hasDW() const { return HasDW; }
  bool hasQW() const { return HasQW; }
  bool hasDWAS() const { return HasDWAS; }
  bool hasQWAS() const { return HasQWAS; }

  // Derived queries
  /// Returns the register width in bits (same as WordSize).
  unsigned getRegWidth() const { return WordSize; }

  /// Returns the stack alignment (same as register width / 8 bytes).
  Align getStackAlign() const { return Align(getRegWidth() / 8); }

  /// Returns the appropriate register class for the word size.
  /// If HasREX, returns the extended 16-register class.
  const TargetRegisterClass *getGPRRegClass() const;

  /// Returns the REX-extended register class if HasREX, else the base class.
  const TargetRegisterClass *getBaseOrRexGPRRegClass() const;

  /// Returns the appropriate pointer register class (16, 32, or 64-bit).
  /// If HasREX, returns the extended 16-register class.
  const TargetRegisterClass *getGPRRegClassForWidth(unsigned Width) const;

  /// Returns the data layout string for this subtarget.
  std::string getDataLayoutString() const { return DLString; }

  /// Register runtime library call implementations (__mulhi3, __divhi3, etc.).
  void initLibcallLoweringInfo(LibcallLoweringInfo &Info) const override;

  /// Build a DataLayout string from raw parameters (WordSize, PtrSize).
  /// Public so ETCATargetMachine::computeDataLayout can delegate to it.
  static std::string buildDataLayoutString(unsigned WordSize, unsigned PtrSize);

private:
  void buildDLString();
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_ETCASUBTARGET_H
