//===-- ETCAAsmPrinter.cpp - ETCA LLVM assembly writer --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ETCASubtarget.h"
#include "ETCATargetMachine.h"
#include "TargetInfo/ETCATargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Compiler.h"

#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"
#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
class ETCAAsmPrinter : public AsmPrinter {
public:
  ETCAAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "ETCA Assembly Printer"; }

  void emitFunctionBodyStart() override {
    // Mark all MBBs referenced by jump tables as needing labels, so their
    // symbols are defined even when the block is a fallthrough predecessor.
    if (auto *MJTI = MF->getJumpTableInfo())
      for (const auto &JT : MJTI->getJumpTables())
        for (auto *MBB : JT.MBBs)
          MBB->setLabelMustBeEmitted();
    AsmPrinter::emitFunctionBodyStart();
  }

  void emitInstruction(const MachineInstr *MI) override {
    // JT_Pseudo is a codegen-only pseudo — expand it into a MOVZI with a
    // label operand, which triggers the encoder's emitMovChain to produce
    // the full MOVZI+SLO placeholder chain with a R_ETCA_MOV_* spanning
    // fixup.  Works for both assembly and object output.
    if (MI->getOpcode() == ETCA::JT_Pseudo) {
      expandJT_Pseudo(MI);
      return;
    }

    // CALLR_Pseudo: expand to a real CALLR with just the register
    // operand.  The first operand is the target register; all variable_ops
    // after it (regmask, implicit args/defs) are dropped for MC emission.
    if (MI->getOpcode() == ETCA::CALLR_Pseudo) {
      expandCALLR_Pseudo(MI);
      return;
    }

    // No LONG_BR_Pseudo to check for — G_BR now emits MOVZI + JMPR
    // directly in the instruction selector.

    MCInst LoweredMI;
    lowerToMCInst(MI, LoweredMI);
    EmitToStreamer(*OutStreamer, LoweredMI);
  }

private:
  /// Expand JT_Pseudo into a MOVZI instruction referencing the jump table
  /// label.  The encoder's emitMovChain produces the full chain.
  void expandJT_Pseudo(const MachineInstr *MI);

  /// Expand CALLR_Pseudo into a CALLR instruction for MC emission.
  void expandCALLR_Pseudo(const MachineInstr *MI);

  /// Lower a standard MachineInstr into an MCInst.
  void lowerToMCInst(const MachineInstr *MI, MCInst &OutMI) const;
};
} // end anonymous namespace

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeETCAAsmPrinter() {
  RegisterAsmPrinter<ETCAAsmPrinter> X(getTheETCATarget());
}

void ETCAAsmPrinter::expandJT_Pseudo(const MachineInstr *MI) {
  assert(MI->getOperand(0).isReg() && "JT_Pseudo dst must be a register");
  assert(MI->getOperand(1).isJTI() && "JT_Pseudo must have JTI operand");

  Register DstReg = MI->getOperand(0).getReg();
  unsigned JTI = MI->getOperand(1).getIndex();
  // Use the default (isLinkerPrivate=false) to match emitJumpTableInfo.
  // On ELF this gives a .L-prefixed internal symbol like .LJTI0_0.
  MCSymbol *JTSym = GetJTISymbol(JTI);

  // Determine MOVZI opcode from pointer width (via DataLayout).
  const DataLayout &DL = MF->getDataLayout();
  unsigned PtrBits = DL.getPointerSizeInBits(0);

  unsigned MovOpcode;
  if (PtrBits >= 64)
    MovOpcode = ETCA::MOVZI64;
  else if (PtrBits >= 32)
    MovOpcode = ETCA::MOVZI32;
  else
    MovOpcode = ETCA::MOVZI16;

  MCInst MCI;
  MCI.setOpcode(MovOpcode);
  MCI.addOperand(MCOperand::createReg(DstReg));
  MCI.addOperand(
      MCOperand::createExpr(MCSymbolRefExpr::create(JTSym, OutContext)));
  EmitToStreamer(*OutStreamer, MCI);
}

void ETCAAsmPrinter::expandCALLR_Pseudo(const MachineInstr *MI) {
  // CALLR_Pseudo operands: [reg (target), variable_ops (regmask,
  // implicit args, implicit-def retval)].
  // Emit a real CALLR with only the register operand.
  MCInst MCI;
  MCI.setOpcode(ETCA::CALLR);
  MCI.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
  EmitToStreamer(*OutStreamer, MCI);
}

void ETCAAsmPrinter::lowerToMCInst(const MachineInstr *MI,
                                   MCInst &OutMI) const {
  unsigned Opcode = MI->getOpcode();

  // Standard lowering: copy opcode and convert operands.
  OutMI.setOpcode(Opcode);

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
      MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
          OutContext.getOrCreateSymbol(MO.getSymbolName()), OutContext));
      break;
    case MachineOperand::MO_JumpTableIndex:
      MCOp = MCOperand::createExpr(MCSymbolRefExpr::create(
          GetJTISymbol(MO.getIndex(), /*isLinkerPrivate=*/true), OutContext));
      break;
    case MachineOperand::MO_RegisterMask:
      // Register masks are only consumed by the register allocator and
      // should not be emitted as MC operands.
      continue;
    }
    OutMI.addOperand(MCOp);
  }
}
