//===- ETCAISelLowering.cpp - ETCA Target Lowering Implementation
//----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ETCA target lowering configuration.  This file only retains the constructor
// (register classes + shared properties) and isLegalAddressingMode.  All
// SDAG-specific operation action setup has been removed — ETCA uses GlobalISel
// exclusively (LegalizerInfo in GISel/ETCALegalizerInfo.cpp handles legality).
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
  // (GISel uses these register classes via RegisterBankInfo and
  // InstructionSelector.)
  unsigned WS = STI.getWordSize();

  if (WS == 64) {
    addRegisterClass(MVT::i64, &GPR64RegClass);
    addRegisterClass(MVT::i32, &GPR32RegClass);
  } else if (WS == 32) {
    addRegisterClass(MVT::i32, &GPR32RegClass);
  }
  addRegisterClass(MVT::i16, &GPRRegClass);
  computeRegisterProperties(STI.getRegisterInfo());

  // Shared target properties used by LLVM infrastructure (not SDAG-specific):
  setBooleanContents(ZeroOrOneBooleanContent);
  setBooleanVectorContents(ZeroOrOneBooleanContent);
  // Jump tables require absolute address materialization (MOVZI+SLO chain
  // with per-slice fixups), which is partially implemented but not yet
  // fully wired.  Set a high threshold to fall back to compare-and-branch
  // chains for now.  To re-enable, implement the fixup types and
  // AsmPrinter expansion in JT_Pseudo, then lower this value.
  setMinimumJumpTableEntries(4);
  setMinFunctionAlignment(Align(2));

  // NOTE: Operation legality (setOperationAction/setLoadExtAction) is NOT set
  // up here — ETCA uses GlobalISel exclusively, where ETCALegalizerInfo in
  // GISel/ETCALegalizerInfo.cpp handles all legalization decisions.

  // The minimum stack argument alignment equals the register width in bytes.
  // This prevents the legalizer's lowerVAArg from generating G_PTRMASK
  // instructions (which ETCA cannot select) for types with alignment at or
  // below the natural stack alignment.
  unsigned RegBytes = WS / 8;
  setMinStackArgumentAlignment(Align(RegBytes));
}

bool ETCATargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                               const AddrMode &AM, Type *Ty,
                                               unsigned AS,
                                               Instruction *I) const {
  // ETCA only supports [reg] addressing.
  // LOAD/STORE instructions have NO immediate offset field — the single
  // register operand provides the full address. Any non-zero offset must
  // be computed via a separate ADDI instruction before the load/store.
  // No GV references, no scaling, no complex addressing.
  return AM.BaseGV == nullptr && AM.HasBaseReg && AM.Scale == 0 &&
         AM.BaseOffs == 0;
}
