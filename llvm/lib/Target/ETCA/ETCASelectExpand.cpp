//===- ETCASelectExpand.cpp - Expand ETCA SELECT_Pseudo ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Expands SELECT_Pseudo instructions into CMP + branch + MOVZ sequences.
// Runs after instruction selection but before register allocation.
//
// SELECT_Pseudo dst, cond, trueval, falseval, pred
//   → MBB: BrOpc TrueBB; fall-through to FalseBB
//   → FalseBB: MOVZ dst, falseval; BR MBBCont
//   → TrueBB: MOVZ dst, trueval; fall-through to MBBCont
//   → MBBCont: rest of function
//
//===----------------------------------------------------------------------===//

#include "ETCA.h"
#include "ETCAInstrInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"

#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"

using namespace llvm;

#define DEBUG_TYPE "etca-select-expand"

namespace {

class ETCASelectExpand : public MachineFunctionPass {
public:
  static char ID;
  ETCASelectExpand() : MachineFunctionPass(ID) {
    initializeETCASelectExpandPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "ETCA SELECT_Pseudo Expansion";
  }
};

char ETCASelectExpand::ID = 0;

} // end anonymous namespace

INITIALIZE_PASS(ETCASelectExpand, "etca-select-expand",
                "ETCA SELECT_Pseudo Expansion", false, false)

FunctionPass *llvm::createETCASelectExpandPass() {
  return new ETCASelectExpand();
}

bool ETCASelectExpand::runOnMachineFunction(MachineFunction &MF) {
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  bool Changed = false;

  // Collect all SELECT_Pseudo instructions first, then expand them.
  // We collect iterators to avoid iterator invalidation during expansion.
  SmallVector<MachineInstr *, 8> ToExpand;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == ETCA::SELECT_Pseudo) {
        ToExpand.push_back(&MI);
      }
    }
  }

  for (MachineInstr *MI : ToExpand) {
    MachineBasicBlock &MBB = *MI->getParent();
    Register Dst = MI->getOperand(0).getReg();
    Register TrueVal = MI->getOperand(2).getReg();
    Register FalseVal = MI->getOperand(3).getReg();
    int64_t Pred = MI->getOperand(4).getImm();
    DebugLoc DL = MI->getDebugLoc();

    // Determine branch instruction.
    unsigned BrOpc;
    switch (Pred) {
    case 0:  BrOpc = ETCA::BNE;  break;
    case 1:  BrOpc = ETCA::BEQ;  break;
    case 2:  BrOpc = ETCA::BNE;  break;
    case 3:  BrOpc = ETCA::BGTU; break;
    case 4:  BrOpc = ETCA::BGEU; break;
    case 5:  BrOpc = ETCA::BLTU; break;
    case 6:  BrOpc = ETCA::BLEU; break;
    case 7:  BrOpc = ETCA::BGT;  break;
    case 8:  BrOpc = ETCA::BGE;  break;
    case 9:  BrOpc = ETCA::BLT;  break;
    case 10: BrOpc = ETCA::BLE;  break;
    default: BrOpc = ETCA::BNE;  break;
    }

    // Split MBB after SELECT_Pseudo.
    MachineBasicBlock *MBBCont = MF.CreateMachineBasicBlock();
    MF.insert(std::next(MBB.getIterator()), MBBCont);

    // Move instructions after SELECT_Pseudo to MBBCont.
    MBBCont->splice(MBBCont->end(), &MBB,
                    std::next(MI->getIterator()), MBB.end());
    MBBCont->transferSuccessorsAndUpdatePHIs(&MBB);

    // Create TrueBB and FalseBB between MBB and MBBCont.
    MachineBasicBlock *TrueBB = MF.CreateMachineBasicBlock();
    MachineBasicBlock *FalseBB = MF.CreateMachineBasicBlock();
    // Insert in order: MBB → FalseBB → TrueBB → MBBCont
    // Use sequential insert after the previous element.
    MF.insert(std::next(MBB.getIterator()), FalseBB);
    MF.insert(std::next(FalseBB->getIterator()), TrueBB);
    // MBB is already in MF.  MBBCont is also in MF (after MBB).
    // After inserting FalseBB after MBB and TrueBB after FalseBB:
    // MBB → FalseBB → TrueBB → ...

    // Set up successors.
    MBB.addSuccessor(TrueBB);
    MBB.addSuccessor(FalseBB);
    FalseBB->addSuccessor(MBBCont);
    TrueBB->addSuccessor(MBBCont);

    // Emit branch in MBB: if condition true, jump to TrueBB.
    // Otherwise, explicitly branch to FalseBB (to handle any block reordering).
    BuildMI(MBB, *MI, DL, TII.get(BrOpc)).addMBB(TrueBB);
    BuildMI(MBB, *MI, DL, TII.get(ETCA::BR)).addMBB(FalseBB);

    // FalseBB: MOVZ dst, falseval; BR MBBCont
    BuildMI(FalseBB, DL, TII.get(ETCA::MOVZ16), Dst).addReg(FalseVal);
    BuildMI(FalseBB, DL, TII.get(ETCA::BR)).addMBB(MBBCont);

    // TrueBB: MOVZ dst, trueval (fall-through to MBBCont)
    BuildMI(TrueBB, DL, TII.get(ETCA::MOVZ16), Dst).addReg(TrueVal);

    // Erase SELECT_Pseudo.
    MI->eraseFromParent();
    Changed = true;
  }

  return Changed;
}
