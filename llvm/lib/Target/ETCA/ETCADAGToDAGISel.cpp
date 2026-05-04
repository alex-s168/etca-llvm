//===- ETCADAGToDAGISel.cpp - ETCA DAG-to-DAG Instruction Selector --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the ETCA-specific DAG-to-DAG instruction selector.
//
// NOTE: This SDAG pass exists only for completeness during development.
// The primary instruction selection path is through GlobalISel.
// Per the project requirements, this SDAG fallback should NOT be used
// in production.  It is kept here for testing and debugging purposes.
//
//===----------------------------------------------------------------------===//

#include "ETCA.h"
#include "ETCAInstrInfo.h"
#include "ETCAISelLowering.h"
#include "ETCATargetMachine.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "etca-isel"

using namespace llvm;
using namespace ETCA;

namespace {
class ETCADAGToDAGISel : public SelectionDAGISel {
public:
  ETCADAGToDAGISel(ETCATargetMachine &TM,
                   CodeGenOptLevel OptLevel = CodeGenOptLevel::Default)
      : SelectionDAGISel(TM, OptLevel) {}

  StringRef getPassName() const {
    return "ETCA DAG-to-DAG Instruction Selection";
  }

  void Select(SDNode *N) override;

  bool SelectInlineAsmMemoryOperand(
      const SDValue &Op, InlineAsm::ConstraintCode ConstraintID,
      std::vector<SDValue> &OutOps) override;

#include "ETCAGenDAGISel.inc"
};

class ETCADAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  ETCADAGToDAGISelLegacy(ETCATargetMachine &TM,
                         CodeGenOptLevel OptLevel = CodeGenOptLevel::Default)
      : SelectionDAGISelLegacy(
            ID, std::make_unique<ETCADAGToDAGISel>(TM, OptLevel)) {}
};
} // namespace

char ETCADAGToDAGISelLegacy::ID;

void ETCADAGToDAGISel::Select(SDNode *N) {
  SDLoc DL(N);

  switch (N->getOpcode()) {
  case ISD::STORE: {
    auto *MemN = cast<StoreSDNode>(N);
    SDValue Val = MemN->getValue();
    SDValue BasePtr = MemN->getBasePtr();
    SDNode *StNode =
        CurDAG->getMachineNode(ETCA::STORE16, DL, MVT::Other, Val, BasePtr);
    ReplaceNode(N, StNode);
    return;
  }
  case ISD::LOAD: {
    if (N->use_empty())
      return;
    SDValue BasePtr = cast<MemSDNode>(N)->getBasePtr();
    SDNode *LdNode =
        CurDAG->getMachineNode(ETCA::LOAD16, DL, MVT::i16, MVT::Other, BasePtr);
    ReplaceNode(N, LdNode);
    return;
  }
  case ISD::BR: {
    SDValue Target = N->getOperand(1);
    SDValue Chain = N->getOperand(0);
    SDNode *BrNode = CurDAG->getMachineNode(
        ETCA::BR, DL, MVT::Other, Target, Chain);
    ReplaceNode(N, BrNode);
    return;
  }
  case ISD::Constant: {
    if (N->use_empty())
      return;
    auto *C = cast<ConstantSDNode>(N);
    int64_t Val = C->getSExtValue();
    MVT VT = N->getSimpleValueType(0);
    unsigned Opc;
    if (VT == MVT::i64)      Opc = MOVZI64;
    else if (VT == MVT::i32) Opc = MOVZI32;
    else                     Opc = MOVZI16;

    SDNode *Mov = CurDAG->getMachineNode(
        Opc, DL, VT, CurDAG->getTargetConstant(Val, DL, VT));
    ReplaceNode(N, Mov);
    return;
  }
  default:
    break;
  }

  if (N->getOpcode() < ISD::BUILTIN_OP_END)
    SelectCode(N);
  if (N->isMachineOpcode())
    return;

  switch (N->getOpcode()) {
  case ISD::FrameIndex: {
    if (N->use_empty())
      return;
    auto *FI = cast<FrameIndexSDNode>(N);
    MVT VT = N->getSimpleValueType(0);
    SDValue TFI = CurDAG->getTargetFrameIndex(FI->getIndex(), VT);
    unsigned Opc;
    if (VT == MVT::i64)      Opc = MOVZI64;
    else if (VT == MVT::i32) Opc = MOVZI32;
    else                     Opc = MOVZI16;
    SDNode *Mov = CurDAG->getMachineNode(Opc, DL, VT, TFI);
    ReplaceNode(N, Mov);
    return;
  }

  case ISD::CALLSEQ_START:
  case ISD::CALLSEQ_END:
    return;

  case ETCAISD::CMP: {
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    ReplaceNode(N,
                CurDAG->getMachineNode(ETCA::CMP, DL, MVT::Glue, LHS, RHS));
    return;
  }
  case ETCAISD::RET_FLAG: {
    SDValue Chain = N->getOperand(0);
    if (N->getNumOperands() > 1 && N->getOperand(1).getValueType() == MVT::Glue)
      ReplaceNode(N, CurDAG->getMachineNode(ETCA::RET_Pseudo, DL, MVT::Other,
                                             Chain, N->getOperand(1)));
    else
      ReplaceNode(N,
                  CurDAG->getMachineNode(ETCA::RET_Pseudo, DL, MVT::Other,
                                         Chain));
    return;
  }
  case ETCAISD::CALL: {
    SmallVector<SDValue, 8> Ops;
    SDValue Callee = N->getOperand(1);
    Ops.push_back(Callee);
    for (unsigned i = 2, e = N->getNumOperands(); i < e; ++i)
      Ops.push_back(N->getOperand(i));
    SDVTList VTs = CurDAG->getVTList(MVT::Other, MVT::Glue);
    SDNode *CallNode =
        CurDAG->getMachineNode(ETCA::CALL_Pseudo, DL, VTs, Ops);
    ReplaceNode(N, CallNode);
    return;
  }
  case ETCAISD::BR_CC: {
    unsigned NumOps = N->getNumOperands();

    if (NumOps == 4 && N->getValueType(0) == MVT::i16 &&
        N->getOperand(0).getValueType() == MVT::i16) {
      // Select form
      SDValue CondVal = N->getOperand(0);
      SDValue TrueVal = N->getOperand(1);
      SDValue FalseVal = N->getOperand(2);
      SDValue Glue = N->getOperand(3);

      SDVTList SelVTs = CurDAG->getVTList(MVT::i16, MVT::Glue);
      SmallVector<SDValue, 4> SelOps;
      SelOps.push_back(CondVal);
      SelOps.push_back(TrueVal);
      SelOps.push_back(FalseVal);
      SelOps.push_back(Glue);
      SDNode *SelNode =
          CurDAG->getMachineNode(ETCA::SELECT_Pseudo, DL, SelVTs, SelOps);
      ReplaceNode(N, SelNode);
      return;
    }

    // Branch form
    SDValue Target = N->getOperand(0);
    SDValue CondVal = N->getOperand(1);
    SDValue Chain = N->getOperand(2);
    SDValue Glue = N->getOperand(3);

    auto *CondC = dyn_cast<ConstantSDNode>(CondVal);
    unsigned ECC = CondC ? CondC->getZExtValue() : 14;

    unsigned BrOpc;
    switch (ECC) {
    case ETCAISD::COND_EQ:  BrOpc = ETCA::BEQ;  break;
    case ETCAISD::COND_NE:  BrOpc = ETCA::BNE;  break;
    case ETCAISD::COND_LT:  BrOpc = ETCA::BLT;  break;
    case ETCAISD::COND_GE:  BrOpc = ETCA::BGE;  break;
    case ETCAISD::COND_ULT: BrOpc = ETCA::BLTU; break;
    case ETCAISD::COND_UGE: BrOpc = ETCA::BGEU; break;
    case ETCAISD::COND_LE:  BrOpc = ETCA::BLE;  break;
    case ETCAISD::COND_GT:  BrOpc = ETCA::BGT;  break;
    case ETCAISD::COND_ULE: BrOpc = ETCA::BLEU; break;
    case ETCAISD::COND_UGT: BrOpc = ETCA::BGTU; break;
    default:
      BrOpc = ETCA::BR;
      break;
    }

    SDNode *BrNode =
        CurDAG->getMachineNode(BrOpc, DL, MVT::Other, Target, Chain, Glue);
    ReplaceNode(N, BrNode);
    return;
  }
  default:
    return;
  }
}

bool ETCADAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintID,
    std::vector<SDValue> &OutOps) {
  SDValue Base = Op;
  OutOps.push_back(Base);
  return false;
}

/// createETCAISelDag - Create the ETCA DAG-to-DAG instruction selector pass.
FunctionPass *llvm::createETCAISelDag(ETCATargetMachine &TM,
                                      CodeGenOptLevel OptLevel) {
  return new ETCADAGToDAGISelLegacy(TM, OptLevel);
}
