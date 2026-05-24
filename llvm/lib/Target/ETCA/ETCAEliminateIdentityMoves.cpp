//===- ETCAEliminateIdentityMoves.cpp - Eliminate MOVZ/MOVS identity copies
//-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Post-register-allocation pass that eliminates MOVZ/MOVS instructions where
// the destination and source physical registers are the same (identity copy)
// AND the instruction width matches the target register width.
//
// Elimination is only safe when the MOVZ/S width equals the register width:
//   RegWidth=64: MOVZ64/MOVS64 only  (SS=11, full register copy)
//   RegWidth=32: MOVZ32/MOVS32 only  (SS=10, full register copy)
//   RegWidth=16: MOVZ16/MOVS16 only  (SS=01, full register copy)
//
// Narrower-width MOVZ/S (e.g. MOVS32 in QW mode) are NOT eliminated because
// they have side effects: they read a narrower value and extend to the full
// register width, changing the high bits.  For example, in QW mode:
//   $d0 = MOVS32 $d0   reads 32 bits from d0, sign-extends to 64 bits,
//                       writes the full 64-bit result to q0 (through d0).
//   Eliminating it would leave garbage in the high 32 bits of q0.
//
// Identity copies arise from several sources:
//   - G_TRUNC followed by COPY: the temp vreg and source vreg may land on the
//     same physical register, making the MOVZ a no-op identity.
//   - G_ZEXT/G_SEXT: same pattern with the extension op.
//   - SELECT_Pseudo expansion: MOVZ of identical true/false values.
//
// Identity copies waste code size (2 bytes each) and add noise to codegen.
// They are harmless but should be removed.
//
//===----------------------------------------------------------------------===//

#include "ETCA.h"
#include "ETCAInstrInfo.h"
#include "ETCASubtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"

#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"

using namespace llvm;

#define DEBUG_TYPE "etca-eliminate-identity-moves"

namespace {

class ETCAEliminateIdentityMoves : public MachineFunctionPass {
public:
  static char ID;
  ETCAEliminateIdentityMoves() : MachineFunctionPass(ID) {
    initializeETCAEliminateIdentityMovesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "ETCA Identity Move Elimination";
  }
};

char ETCAEliminateIdentityMoves::ID = 0;

} // end anonymous namespace

INITIALIZE_PASS(ETCAEliminateIdentityMoves, "etca-eliminate-identity-moves",
                "ETCA Identity Move Elimination", false, false)

FunctionPass *llvm::createETCAEliminateIdentityMovesPass() {
  return new ETCAEliminateIdentityMoves();
}

// Return true if the given opcode is a MOVZ or MOVS whose operand width
// matches the target register width.  Only these can be safely eliminated
// as identities (same dst/src) because narrower-width MOVZ/S instructions
// extend the value to the full register width.
static bool isFullWidthCopy(unsigned Opc, unsigned RegWidth) {
  switch (Opc) {
  case ETCA::MOVZ16:
  case ETCA::MOVS16:
    return RegWidth == 16;
  case ETCA::MOVZ32:
  case ETCA::MOVS32:
    return RegWidth == 32;
  case ETCA::MOVZ64:
  case ETCA::MOVS64:
    return RegWidth == 64;
  }
  return false;
}

bool ETCAEliminateIdentityMoves::runOnMachineFunction(MachineFunction &MF) {
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  unsigned RegWidth = ST.getRegWidth();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto MI = MBB.begin(); MI != MBB.end();) {
      bool Erased = false;
      if (isFullWidthCopy(MI->getOpcode(), RegWidth) &&
          MI->getOperand(0).isReg() && MI->getOperand(1).isReg() &&
          MI->getOperand(0).getReg() == MI->getOperand(1).getReg()) {
        LLVM_DEBUG(dbgs() << "Erasing identity move: " << *MI);
        MI = MBB.erase(MI);
        Erased = true;
        Changed = true;
      }
      if (!Erased)
        ++MI;
    }
  }

  return Changed;
}
