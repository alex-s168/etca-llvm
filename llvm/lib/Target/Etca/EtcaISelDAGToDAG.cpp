//====-- EtcaISelDAGToDAG.cpp - A dag to dag inst selector for Etca ------====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the Etca target.
//
//===----------------------------------------------------------------------===//

#include "Etca.h"
#include "EtcaTargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "etca-isel"
#define PASS_NAME "Etca DAG->DAG Pattern Instruction Selection"

namespace {

class EtcaDAGToDAGISel : public SelectionDAGISel {
public:
  EtcaDAGToDAGISel(EtcaTargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

  void Select(SDNode *Node) override;

  bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                    InlineAsm::ConstraintCode ConstraintID,
                                    std::vector<SDValue> &OutOps) override;

  // Include the pieces autogenerated from the target description.
#include "EtcaGenDAGISel.inc"
}; // namespace

class EtcaDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;

  EtcaDAGToDAGISelLegacy(EtcaTargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISelLegacy(
            ID, std::make_unique<EtcaDAGToDAGISel>(TM, OptLevel)) {}

  StringRef getPassName() const override {
    return "Etca DAG->DAG Pattern Instruction Selection";
  }
};
} // end anonymous namespace

char EtcaDAGToDAGISelLegacy::ID = 0;

FunctionPass *llvm::createEtcaISelDag(EtcaTargetMachine &TM,
                                      CodeGenOptLevel OptLevel) {
  return new EtcaDAGToDAGISelLegacy(TM, OptLevel);
}

void EtcaDAGToDAGISel::Select(SDNode *Node) {
  SDLoc DL(Node);
  unsigned Opcode = Node->getOpcode();

  if (Opcode == ISD::FrameIndex) {
    SDLoc DL(Node);
    SDValue Imm = CurDAG->getTargetConstant(0, DL, MVT::i16);
    int FI = cast<FrameIndexSDNode>(Node)->getIndex();
    EVT VT = Node->getValueType(0);
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, VT);
    ReplaceNode(Node, CurDAG->getMachineNode(Etca::ADD_RI, DL, VT, TFI, Imm));
    return;
  }

  if (Opcode == EtcaISD::RET) {
    SDLoc DL(Node);
    EVT VT = Node->getValueType(0);
    ReplaceNode(Node, 
            CurDAG->getMachineNode(Etca::JMP_REG, DL, VT,
                CurDAG->getRegister(Etca::LN, MVT::i16)));
    return;
  }

  SelectCode(Node);
}

bool EtcaDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintID,
    std::vector<SDValue> &OutOps) {
  assert(false && "unsupported");
}
