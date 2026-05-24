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

  // Ensure $r6 (SP) is a live-in to every basic block.  $r6 is a reserved
  // register (set up by the prologue) and is used implicitly by call
  // instructions (via ADJCALLSTACKDOWN/UP).  LLVM's LiveIntervals requires
  // that any physical register used in a block is either defined there or
  // listed as live-in.  Without this, empty intermediate blocks in the CFG
  // (e.g. a block that just branches to another block) would lack $r6 as
  // live-in, causing:
  //   "The register $r6 needs to be live in to %bb.N, but is missing from
  //    the live-in list"
  // This is safe because $r6 is reserved and never allocated to a vreg.
  if (ST.hasSAF()) {
    for (MachineBasicBlock &MBB : MF) {
      if (!MBB.isLiveIn(ETCA::R6)) {
        MBB.addLiveIn(ETCA::R6);
        Changed = true;
      }
    }
  }

  // Expand any remaining ICMP_Pseudo markers.  These are created during
  // instruction selection (by the G_ICMP handler) and normally consumed
  // by the G_BRCOND / G_SELECT handlers.  If no such consumer exists
  // (e.g. the ICMP result is returned directly as i1), the ICMP_Pseudo's
  // result register still has uses (from G_ZEXT/G_ANYEXT/G_TRUNC).  We
  // must materialize the condition value (0 or 1) using a branch + PHI
  // sequence similar to SELECT_Pseudo expansion.
  //
  // Expansion:
  //   ICMP_Pseudo %dst, %pred
  //   → TrueBB: MOVZ %tmptrue, 1; fall-through to MBBCont
  //   → FalseBB: MOVZ %tmpfalse, 0; BR MBBCont
  //   → MBBCont: %dst = PHI %tmpfalse (FalseBB), %tmptrue (TrueBB)
  //
  // The CMP instruction before the ICMP_Pseudo has already set the
  // condition flags.
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

    // Check if the ICMP_Pseudo result has any non-dead uses.
    // Uses from SELECT_Pseudo (cond operand) are dead — SELECT_Pseudo
    // only checks if its cond def is ICMP_Pseudo to know CMP was emitted.
    // Uses from anything else (G_ZEXT, G_TRUNC, etc.) are real and
    // require the condition value to be materialized.
    bool HasRealUse = false;
    for (MachineOperand &MO : MRI.use_operands(Dst)) {
      MachineInstr &User = *MO.getParent();
      if (User.getOpcode() != ETCA::SELECT_Pseudo) {
        HasRealUse = true;
        break;
      }
    }

    // If no real uses, just erase the ICMP_Pseudo (it was consumed by
    // a G_BRCOND or G_SELECT that read its predicate).
    if (!HasRealUse) {
      MI->eraseFromParent();
      Changed = true;
      continue;
    }

    // Determine branch instruction for the inverse predicate.
    // The branch is "branch-if-condition-false" so we jump to FalseBB
    // when the condition does NOT hold.  TrueBB is fall-through if the
    // condition holds (CMP has already set the flags).
    unsigned BrOpc;
    switch (Pred) {
    case 1:
      BrOpc = ETCA::BNE;
      break; // EQ → branch if NOT equal (Z=0)
    case 2:
      BrOpc = ETCA::BEQ;
      break; // NE → branch if equal (Z=1)
    case 3:
      BrOpc = ETCA::BLEU;
      break; // UGT → branch if ULE
    case 4:
      BrOpc = ETCA::BLTU;
      break; // UGE → branch if ULT
    case 5:
      BrOpc = ETCA::BGEU;
      break; // ULT → branch if UGE
    case 6:
      BrOpc = ETCA::BGTU;
      break; // ULE → branch if UGT
    case 7:
      BrOpc = ETCA::BLE;
      break; // SGT → branch if SLE
    case 8:
      BrOpc = ETCA::BLT;
      break; // SGE → branch if SLT
    case 9:
      BrOpc = ETCA::BGE;
      break; // SLT → branch if SGE
    case 10:
      BrOpc = ETCA::BGT;
      break; // SLE → branch if SGT
    default:
      BrOpc = ETCA::BEQ;
      break;
    }

    // Split MBB after ICMP_Pseudo.
    MachineBasicBlock *MBBCont = MF.CreateMachineBasicBlock();
    MF.insert(std::next(MBB.getIterator()), MBBCont);

    // Move instructions after ICMP_Pseudo to MBBCont.
    MBBCont->splice(MBBCont->end(), &MBB, std::next(MI->getIterator()),
                    MBB.end());
    MBBCont->transferSuccessorsAndUpdatePHIs(&MBB);

    // Create TrueBB (condition holds → result=1) and FalseBB (condition
    // does not hold → result=0) between MBB and MBBCont.
    MachineBasicBlock *TrueBB = MF.CreateMachineBasicBlock();
    MachineBasicBlock *FalseBB = MF.CreateMachineBasicBlock();
    // Insert: MBB → FalseBB → TrueBB → MBBCont
    MF.insert(std::next(MBB.getIterator()), FalseBB);
    MF.insert(std::next(FalseBB->getIterator()), TrueBB);

    // Propagate $r6 (SP) as live-in to new blocks.
    MBBCont->addLiveIn(ETCA::R6);
    FalseBB->addLiveIn(ETCA::R6);
    TrueBB->addLiveIn(ETCA::R6);

    // Set up successors.
    MBB.addSuccessor(TrueBB);
    MBB.addSuccessor(FalseBB);
    FalseBB->addSuccessor(MBBCont);
    TrueBB->addSuccessor(MBBCont);

    // Emit branch: if condition does NOT hold, jump to FalseBB.
    // Otherwise fall through to TrueBB.
    BuildMI(MBB, *MI, DL, TII.get(BrOpc)).addMBB(FalseBB);
    BuildMI(MBB, *MI, DL, TII.get(ETCA::BR)).addMBB(TrueBB);

    // Create temporary virtual registers for SSA form.
    const TargetRegisterClass *DstRC = MRI.getRegClass(Dst);
    Register TmpFalse = MRI.createVirtualRegister(DstRC);
    Register TmpTrue = MRI.createVirtualRegister(DstRC);

    // FalseBB: MOVZ %tmpfalse, 0; BR MBBCont
    BuildMI(FalseBB, DL,
            TII.get(getMovzOpcForRegWidth(
                MF.getSubtarget<ETCASubtarget>().getRegWidth())),
            TmpFalse)
        .addImm(0);
    BuildMI(FalseBB, DL, TII.get(ETCA::BR)).addMBB(MBBCont);

    // TrueBB: MOVZ %tmptrue, 1 (fall-through to MBBCont)
    BuildMI(TrueBB, DL,
            TII.get(getMovzOpcForRegWidth(
                MF.getSubtarget<ETCASubtarget>().getRegWidth())),
            TmpTrue)
        .addImm(1);

    // MBBCont: %dst = PHI %tmpfalse (FalseBB), %tmptrue (TrueBB)
    BuildMI(*MBBCont, MBBCont->begin(), DL, TII.get(TargetOpcode::PHI), Dst)
        .addReg(TmpFalse)
        .addMBB(FalseBB)
        .addReg(TmpTrue)
        .addMBB(TrueBB);

    // Erase ICMP_Pseudo.
    MI->eraseFromParent();
    Changed = true;
  }

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

    // Split MBB after SELECT_Pseudo.
    MachineBasicBlock *MBBCont = MF.CreateMachineBasicBlock();
    MF.insert(std::next(MBB.getIterator()), MBBCont);

    // Move instructions after SELECT_Pseudo to MBBCont.
    MBBCont->splice(MBBCont->end(), &MBB, std::next(MI->getIterator()),
                    MBB.end());
    MBBCont->transferSuccessorsAndUpdatePHIs(&MBB);

    // Create TrueBB and FalseBB between MBB and MBBCont.
    MachineBasicBlock *TrueBB = MF.CreateMachineBasicBlock();
    MachineBasicBlock *FalseBB = MF.CreateMachineBasicBlock();
    // Insert in order: MBB → FalseBB → TrueBB → MBBCont
    // Use sequential insert after the previous element.
    MF.insert(std::next(MBB.getIterator()), FalseBB);
    MF.insert(std::next(FalseBB->getIterator()), TrueBB);

    // Propagate $r6 (SP) as live-in to all new blocks.  The original MBB
    // always has $r6 live (it is set up in the prologue), and the blocks
    // created here may contain instructions that reference SP (e.g.
    // ADJCALLSTACKDOWN/UP from function calls after the SELECT_Pseudo).
    // Without this, LiveIntervals fails with "The register $r6 needs to
    // be live in to %bb.N, but is missing from the live-in list".
    MBBCont->addLiveIn(ETCA::R6);
    FalseBB->addLiveIn(ETCA::R6);
    TrueBB->addLiveIn(ETCA::R6);
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

    // Create temporary virtual registers to maintain SSA form.
    // Using Dst directly in both MOVZ instructions would create
    // two definitions of Dst, violating SSA (causes "getVRegDef
    // assumes at most one definition" assertion in later passes).
    const TargetRegisterClass *DstRC = MRI.getRegClass(Dst);
    Register TmpFalse = MRI.createVirtualRegister(DstRC);
    Register TmpTrue = MRI.createVirtualRegister(DstRC);

    // FalseBB: MOVZ tmpfalse, falseval; BR MBBCont
    BuildMI(FalseBB, DL,
            TII.get(getMovzOpcForRegWidth(
                MF.getSubtarget<ETCASubtarget>().getRegWidth())),
            TmpFalse)
        .addReg(FalseVal);
    BuildMI(FalseBB, DL, TII.get(ETCA::BR)).addMBB(MBBCont);

    // TrueBB: MOVZ tmptrue, trueval (fall-through to MBBCont)
    BuildMI(TrueBB, DL,
            TII.get(getMovzOpcForRegWidth(
                MF.getSubtarget<ETCASubtarget>().getRegWidth())),
            TmpTrue)
        .addReg(TrueVal);

    // MBBCont: Dst = PHI TmpFalse (FalseBB), TmpTrue (TrueBB)
    // This PHI joins the two SSA definitions and is later eliminated
    // by the standard phi elimination pass.
    BuildMI(*MBBCont, MBBCont->begin(), DL, TII.get(TargetOpcode::PHI), Dst)
        .addReg(TmpFalse)
        .addMBB(FalseBB)
        .addReg(TmpTrue)
        .addMBB(TrueBB);

    // Erase SELECT_Pseudo.
    MI->eraseFromParent();
    Changed = true;
  }

  return Changed;
}
