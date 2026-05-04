//===-- ETCAAsmPrinter.cpp - ETCA LLVM assembly writer --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ETCATargetMachine.h"
#include "TargetInfo/ETCATargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class ETCAAsmPrinter : public AsmPrinter {
public:
  ETCAAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "ETCA Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override {
    MCInst LoweredMI;
    LowerETCAMPEMCInst(MI, LoweredMI);
    EmitToStreamer(*OutStreamer, LoweredMI);
  }

private:
  void LowerETCAMPEMCInst(const MachineInstr *MI, MCInst &OutMI) const;
};
} // end anonymous namespace

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeETCAAsmPrinter() {
  RegisterAsmPrinter<ETCAAsmPrinter> X(getTheETCATarget());
}

void ETCAAsmPrinter::LowerETCAMPEMCInst(const MachineInstr *MI,
                                        MCInst &OutMI) const {
  OutMI.setOpcode(MI->getOpcode());

  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    MCOperand MCOp;
    switch (MO.getType()) {
    default:
      llvm_unreachable("unknown operand type in ETCAAsmPrinter");
    case MachineOperand::MO_Register:
      MCOp = MCOperand::createReg(MO.getReg());
      break;
    case MachineOperand::MO_Immediate:
      MCOp = MCOperand::createImm(MO.getImm());
      break;
    case MachineOperand::MO_MachineBasicBlock:
      MCOp = MCOperand::createExpr(
          MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), OutContext));
      break;
    case MachineOperand::MO_FrameIndex:
      MCOp = MCOperand::createImm(MO.getIndex());
      break;
    case MachineOperand::MO_GlobalAddress:
      MCOp = MCOperand::createExpr(
          MCSymbolRefExpr::create(getSymbol(MO.getGlobal()), OutContext));
      break;
    case MachineOperand::MO_ExternalSymbol:
      MCOp = MCOperand::createExpr(
          MCSymbolRefExpr::create(
              OutContext.getOrCreateSymbol(MO.getSymbolName()), OutContext));
      break;
    }
    OutMI.addOperand(MCOp);
  }
}
