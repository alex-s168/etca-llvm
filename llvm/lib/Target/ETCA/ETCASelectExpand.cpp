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
//   → MBB: BrOpc TrueBB; BR FalseBB (fall-through)
//   → FalseBB: MOVZ tmpfalse, falseval; BR MBBCont
//   → TrueBB:  MOVZ tmptrue, trueval; fall-through to MBBCont
//   → MBBCont: Dst = PHI tmpfalse (FalseBB), tmptrue (TrueBB)
//
// The PHI maintains SSA form (two separate vregs in TrueBB/FalseBB),
// avoiding "getVRegDef assumes at most one definition" assertion.
//
//===----------------------------------------------------------------------===//

#include "ETCA.h"
#include "ETCAInstrInfo.h"
#include "ETCARegisterInfo.h"
#include "ETCASubtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"

#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"

#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"

using namespace llvm;

/// Return the MOVZ opcode for the given register width.
static unsigned getMovzOpcForRegWidth(unsigned RegWidth) {
  switch (RegWidth) {
  case 64:
    return ETCA::MOVZ64;
  case 32:
    return ETCA::MOVZ32;
  default:
    return ETCA::MOVZ16;
  }
}

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
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  bool Changed = false;

  // Ensure $r6 (SP) is a live-in to every basic block.
  if (ST.hasSAF()) {
    for (MachineBasicBlock &MBB : MF) {
      if (!MBB.isLiveIn(ETCA::R6)) {
        MBB.addLiveIn(ETCA::R6);
        Changed = true;
      }
    }
  }

  // Expand any remaining ICMP_Pseudo markers.
  MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<MachineInstr *, 8> ICMPToExpand;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == ETCA::ICMP_Pseudo) {
        ICMPToExpand.push_back(&MI);
      }
    }
  }

  for (MachineInstr *MI : ICMPToExpand) {
    MachineBasicBlock &MBB = *MI->getParent();
    Register Dst = MI->getOperand(0).getReg();
    int64_t Pred = MI->getOperand(1).getImm();
    DebugLoc DL = MI->getDebugLoc();

    bool HasRealUse = false;
    for (MachineOperand &MO : MRI.use_operands(Dst)) {
      MachineInstr &User = *MO.getParent();
      if (User.getOpcode() != ETCA::SELECT_Pseudo) {
        HasRealUse = true;
        break;
      }
    }

    if (!HasRealUse) {
      MI->eraseFromParent();
      Changed = true;
      continue;
    }

    unsigned BrOpc;
    switch (Pred) {
    case 1:
      BrOpc = ETCA::BNE;
      break;
    case 2:
      BrOpc = ETCA::BEQ;
      break;
    case 3:
      BrOpc = ETCA::BLEU;
      break;
    case 4:
      BrOpc = ETCA::BLTU;
      break;
    case 5:
      BrOpc = ETCA::BGEU;
      break;
    case 6:
      BrOpc = ETCA::BGTU;
      break;
    case 7:
      BrOpc = ETCA::BLE;
      break;
    case 8:
      BrOpc = ETCA::BLT;
      break;
    case 9:
      BrOpc = ETCA::BGE;
      break;
    case 10:
      BrOpc = ETCA::BGT;
      break;
    default:
      BrOpc = ETCA::BEQ;
      break;
    }

    MachineBasicBlock *MBBCont = MF.CreateMachineBasicBlock();
    MF.insert(std::next(MBB.getIterator()), MBBCont);
    MBBCont->splice(MBBCont->end(), &MBB, std::next(MI->getIterator()),
                    MBB.end());
    MBBCont->transferSuccessorsAndUpdatePHIs(&MBB);

    MachineBasicBlock *TrueBB = MF.CreateMachineBasicBlock();
    MachineBasicBlock *FalseBB = MF.CreateMachineBasicBlock();
    MF.insert(std::next(MBB.getIterator()), FalseBB);
    MF.insert(std::next(FalseBB->getIterator()), TrueBB);

    MBBCont->addLiveIn(ETCA::R6);
    FalseBB->addLiveIn(ETCA::R6);
    TrueBB->addLiveIn(ETCA::R6);

    MBB.addSuccessor(TrueBB);
    MBB.addSuccessor(FalseBB);
    FalseBB->addSuccessor(MBBCont);
    TrueBB->addSuccessor(MBBCont);

    BuildMI(MBB, *MI, DL, TII.get(BrOpc)).addMBB(FalseBB);
    BuildMI(MBB, *MI, DL, TII.get(ETCA::BR)).addMBB(TrueBB);

    const TargetRegisterClass *DstRC = MRI.getRegClass(Dst);
    Register TmpFalse = MRI.createVirtualRegister(DstRC);
    Register TmpTrue = MRI.createVirtualRegister(DstRC);

    BuildMI(FalseBB, DL,
            TII.get(getMovzOpcForRegWidth(
                MF.getSubtarget<ETCASubtarget>().getRegWidth())),
            TmpFalse)
        .addImm(0);
    BuildMI(FalseBB, DL, TII.get(ETCA::BR)).addMBB(MBBCont);

    BuildMI(TrueBB, DL,
            TII.get(getMovzOpcForRegWidth(
                MF.getSubtarget<ETCASubtarget>().getRegWidth())),
            TmpTrue)
        .addImm(1);

    BuildMI(*MBBCont, MBBCont->begin(), DL, TII.get(TargetOpcode::PHI), Dst)
        .addReg(TmpFalse)
        .addMBB(FalseBB)
        .addReg(TmpTrue)
        .addMBB(TrueBB);

    MI->eraseFromParent();
    Changed = true;
  }

  // Collect all SELECT_Pseudo instructions first, then expand them.
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

    unsigned BrOpc;
    switch (Pred) {
    case 0:
      BrOpc = ETCA::BNE;
      break;
    case 1:
      BrOpc = ETCA::BEQ;
      break;
    case 2:
      BrOpc = ETCA::BNE;
      break;
    case 3:
      BrOpc = ETCA::BGTU;
      break;
    case 4:
      BrOpc = ETCA::BGEU;
      break;
    case 5:
      BrOpc = ETCA::BLTU;
      break;
    case 6:
      BrOpc = ETCA::BLEU;
      break;
    case 7:
      BrOpc = ETCA::BGT;
      break;
    case 8:
      BrOpc = ETCA::BGE;
      break;
    case 9:
      BrOpc = ETCA::BLT;
      break;
    case 10:
      BrOpc = ETCA::BLE;
      break;
    default:
      BrOpc = ETCA::BNE;
      break;
    }

    MachineBasicBlock *MBBCont = MF.CreateMachineBasicBlock();
    MF.insert(std::next(MBB.getIterator()), MBBCont);
    MBBCont->splice(MBBCont->end(), &MBB, std::next(MI->getIterator()),
                    MBB.end());
    MBBCont->transferSuccessorsAndUpdatePHIs(&MBB);

    MachineBasicBlock *TrueBB = MF.CreateMachineBasicBlock();
    MachineBasicBlock *FalseBB = MF.CreateMachineBasicBlock();
    MF.insert(std::next(MBB.getIterator()), FalseBB);
    MF.insert(std::next(FalseBB->getIterator()), TrueBB);

    MBBCont->addLiveIn(ETCA::R6);
    FalseBB->addLiveIn(ETCA::R6);
    TrueBB->addLiveIn(ETCA::R6);

    MBB.addSuccessor(TrueBB);
    MBB.addSuccessor(FalseBB);
    FalseBB->addSuccessor(MBBCont);
    TrueBB->addSuccessor(MBBCont);

    BuildMI(MBB, *MI, DL, TII.get(BrOpc)).addMBB(TrueBB);
    BuildMI(MBB, *MI, DL, TII.get(ETCA::BR)).addMBB(FalseBB);

    const TargetRegisterClass *DstRC = MRI.getRegClass(Dst);
    Register TmpFalse = MRI.createVirtualRegister(DstRC);
    Register TmpTrue = MRI.createVirtualRegister(DstRC);

    BuildMI(FalseBB, DL,
            TII.get(getMovzOpcForRegWidth(
                MF.getSubtarget<ETCASubtarget>().getRegWidth())),
            TmpFalse)
        .addReg(FalseVal);
    BuildMI(FalseBB, DL, TII.get(ETCA::BR)).addMBB(MBBCont);

    BuildMI(TrueBB, DL,
            TII.get(getMovzOpcForRegWidth(
                MF.getSubtarget<ETCASubtarget>().getRegWidth())),
            TmpTrue)
        .addReg(TrueVal);

    BuildMI(*MBBCont, MBBCont->begin(), DL, TII.get(TargetOpcode::PHI), Dst)
        .addReg(TmpFalse)
        .addMBB(FalseBB)
        .addReg(TmpTrue)
        .addMBB(TrueBB);

    MI->eraseFromParent();
    Changed = true;
  }

  return Changed;
}
