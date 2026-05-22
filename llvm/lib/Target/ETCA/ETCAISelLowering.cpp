//===- ETCAISelLowering.cpp - ETCA DAG Lowering Implementation ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ETCA target lowering.
//
// Configures which operations are legal/expand/custom for the ETCA target.
// This is used by both GISel (via Legality check). The constructor registers
// register classes and sets operation actions for all supported types and
// CPU variants.
//
// NOTE: SDAG lowering hooks (LowerOperation, LowerFormalArguments, etc.) have
// been deliberately removed. ETCA uses GlobalISel exclusively.
//
//===----------------------------------------------------------------------===//

#include "ETCAISelLowering.h"
#include "ETCARegisterInfo.h"
#include "ETCASubtarget.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

#define DEBUG_TYPE "etca-lower"

// Bring in ETCA-specific enums (not in the PCH)
#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"
#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"

using namespace llvm;
using namespace ETCA;

ETCATargetLowering::ETCATargetLowering(const TargetMachine &TM,
                                       const ETCASubtarget &STI)
    : TargetLowering(TM, STI), STI(STI) {
  // Register the appropriate register classes based on word size.
  unsigned WS = STI.getWordSize();

  if (WS == 64) {
    addRegisterClass(MVT::i64, &GPR64RegClass);
    addRegisterClass(MVT::i32, &GPR32RegClass);
  } else if (WS == 32) {
    addRegisterClass(MVT::i32, &GPR32RegClass);
  }
  addRegisterClass(MVT::i16, &GPRRegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  // Set up type actions for operations based on register width.
  // The legal types are determined by WS.

  // Load extension actions
  if (WS <= 16) {
    setLoadExtAction(ISD::ZEXTLOAD, MVT::i16, MVT::i8, Expand);
    setLoadExtAction(ISD::SEXTLOAD, MVT::i16, MVT::i8, Expand);
    setLoadExtAction(ISD::EXTLOAD, MVT::i16, MVT::i8, Expand);
  }

  // MUL/DIV are not natively supported in base ISA.
  // Use libcalls (compiler-rt __mulhi3, __divhi3, etc.).
  if (WS == 64) {
    setOperationAction(ISD::MUL, MVT::i64, LibCall);
    setOperationAction(ISD::UDIV, MVT::i64, LibCall);
    setOperationAction(ISD::SDIV, MVT::i64, LibCall);
    setOperationAction(ISD::UREM, MVT::i64, LibCall);
    setOperationAction(ISD::SREM, MVT::i64, LibCall);
  }
  if (WS >= 32) {
    setOperationAction(ISD::MUL, MVT::i32, LibCall);
    setOperationAction(ISD::UDIV, MVT::i32, LibCall);
    setOperationAction(ISD::SDIV, MVT::i32, LibCall);
    setOperationAction(ISD::UREM, MVT::i32, LibCall);
    setOperationAction(ISD::SREM, MVT::i32, LibCall);
  }
  setOperationAction(ISD::MUL, MVT::i16, LibCall);
  setOperationAction(ISD::UDIV, MVT::i16, LibCall);
  setOperationAction(ISD::SDIV, MVT::i16, LibCall);
  setOperationAction(ISD::UREM, MVT::i16, LibCall);
  setOperationAction(ISD::SREM, MVT::i16, LibCall);

  setOperationAction(ISD::MULHS, MVT::i16, Expand);
  setOperationAction(ISD::MULHU, MVT::i16, Expand);

  // Shift operations
  if (WS == 64) {
    setOperationAction(ISD::SHL, MVT::i64, Legal);
    setOperationAction(ISD::SRA, MVT::i64, Expand);
    setOperationAction(ISD::SRL, MVT::i64, Expand);
  }
  if (WS >= 32) {
    setOperationAction(ISD::SHL, MVT::i32, Legal);
    setOperationAction(ISD::SRA, MVT::i32, Expand);
    setOperationAction(ISD::SRL, MVT::i32, Expand);
  }
  setOperationAction(ISD::SHL, MVT::i16, Legal);
  setOperationAction(ISD::SRA, MVT::i16, Expand);
  setOperationAction(ISD::SRL, MVT::i16, Expand);

  // BR_CC / BRCOND
  if (WS == 64) {
    setOperationAction(ISD::SETCC, MVT::i64, Expand);
    setOperationAction(ISD::BR_CC, MVT::i64, Custom);
    setOperationAction(ISD::BRCOND, MVT::i64, Custom);
    setOperationAction(ISD::SELECT_CC, MVT::i64, Expand);
    setOperationAction(ISD::GlobalAddress, MVT::i64, Custom);
    setOperationAction(ISD::ConstantPool, MVT::i64, Custom);
    setOperationAction(ISD::JumpTable, MVT::i64, Custom);
  }
  if (WS >= 32) {
    setOperationAction(ISD::SETCC, MVT::i32, Expand);
    setOperationAction(ISD::BR_CC, MVT::i32, Custom);
    setOperationAction(ISD::BRCOND, MVT::i32, Custom);
    setOperationAction(ISD::SELECT_CC, MVT::i32, Expand);
    setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);
    setOperationAction(ISD::ConstantPool, MVT::i32, Custom);
    setOperationAction(ISD::JumpTable, MVT::i32, Custom);
  }
  setOperationAction(ISD::SETCC, MVT::i16, Expand);
  setOperationAction(ISD::BR_CC, MVT::i16, Custom);
  setOperationAction(ISD::BRCOND, MVT::i16, Custom);
  setOperationAction(ISD::BRCOND, MVT::i1, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i16, Expand);
  setOperationAction(ISD::GlobalAddress, MVT::i16, Custom);
  setOperationAction(ISD::ConstantPool, MVT::i16, Custom);
  setOperationAction(ISD::JumpTable, MVT::i16, Custom);

  setBooleanContents(ZeroOrOneBooleanContent);
  setBooleanVectorContents(ZeroOrOneBooleanContent);
  setMinimumJumpTableEntries(5);
  setMinFunctionAlignment(Align(2));
}

bool ETCATargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                               const AddrMode &AM, Type *Ty,
                                               unsigned AS,
                                               Instruction *I) const {
  // ETCA only supports [reg] or [reg+small_imm] addressing.
  // Small immediates (within [-16, 15]) can be folded into ADDI+LOAD/STORE.
  // No GV references, no scaling, no complex scaling, no complex addressing.
  return AM.BaseGV == nullptr && AM.HasBaseReg && AM.Scale == 0 &&
         AM.BaseOffs >= -16 && AM.BaseOffs <= 15;
}
