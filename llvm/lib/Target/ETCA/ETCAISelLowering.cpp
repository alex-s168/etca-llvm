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
// This is used by both GISel (via Legality check) and SDAG (not used, per
// the requirement of GISel-only).
//
//===----------------------------------------------------------------------===//

#include "ETCAISelLowering.h"
#include "ETCA.h"
#include "ETCARegisterInfo.h"
#include "ETCASubtarget.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetCallingConv.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Intrinsics.h"
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

static unsigned getETCACondCode(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
    return ETCAISD::COND_EQ;
  case ISD::SETNE:
    return ETCAISD::COND_NE;
  case ISD::SETLT:
    return ETCAISD::COND_LT;
  case ISD::SETGE:
    return ETCAISD::COND_GE;
  case ISD::SETULT:
    return ETCAISD::COND_ULT;
  case ISD::SETUGE:
    return ETCAISD::COND_UGE;
  case ISD::SETLE:
    return ETCAISD::COND_LE;
  case ISD::SETGT:
    return ETCAISD::COND_GT;
  case ISD::SETULE:
    return ETCAISD::COND_ULE;
  case ISD::SETUGT:
    return ETCAISD::COND_UGT;
  default:
    llvm_unreachable("Unknown condition code for ETCA");
  }
}

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

const char *ETCATargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch ((ETCAISD::NodeType)Opcode) {
  case ETCAISD::CMP:
    return "ETCAISD::CMP";
  case ETCAISD::RET_FLAG:
    return "ETCAISD::RET_FLAG";
  case ETCAISD::CALL:
    return "ETCAISD::CALL";
  case ETCAISD::BR_CC:
    return "ETCAISD::BR_CC";
  }
  return nullptr;
}

SDValue ETCATargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:
  case ISD::ConstantPool:
  case ISD::JumpTable:
    return Op;
  case ISD::BR_CC:
    return LowerBR_CC(Op, DAG);
  case ISD::BRCOND:
    return LowerBRCOND(Op, DAG);
  case ETCAISD::CMP:
  case ETCAISD::RET_FLAG:
  case ETCAISD::CALL:
  case ETCAISD::BR_CC:
    return Op;
  default:
    llvm_unreachable("Unimplemented custom lowering op");
  }
}

void ETCATargetLowering::ReplaceNodeResults(SDNode *N,
                                            SmallVectorImpl<SDValue> &Results,
                                            SelectionDAG &DAG) const {}

//===----------------------------------------------------------------------===//
//  Lowering helpers
//===----------------------------------------------------------------------===//

SDValue ETCATargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Target = Op.getOperand(4);

  unsigned ETCACond = getETCACondCode(CC);

  MVT VT = LHS.getSimpleValueType();
  SDValue CmpGlue = DAG.getNode(ETCAISD::CMP, DL, MVT::Glue, LHS, RHS);

  SDValue CondVal = DAG.getConstant(ETCACond, DL, MVT::i16);
  SDValue Ops[] = {Target, CondVal, Chain, CmpGlue};
  SDVTList BrVTs = DAG.getVTList(MVT::Other);
  return DAG.getNode(ETCAISD::BR_CC, DL, BrVTs, Ops);
}

SDValue ETCATargetLowering::LowerBRCOND(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue Cond = Op.getOperand(1);
  SDValue Target = Op.getOperand(2);

  MVT VT = Cond.getSimpleValueType();
  SDValue Zero = DAG.getConstant(0, DL, VT);
  SDValue CmpGlue = DAG.getNode(ETCAISD::CMP, DL, MVT::Glue, Cond, Zero);

  SDValue CondVal = DAG.getConstant(ETCAISD::COND_NE, DL, MVT::i16);
  SDValue Ops[] = {Target, CondVal, Chain, CmpGlue};
  SDVTList BrVTs = DAG.getVTList(MVT::Other);
  return DAG.getNode(ETCAISD::BR_CC, DL, BrVTs, Ops);
}

//===----------------------------------------------------------------------===//
//  Calling Convention
//===----------------------------------------------------------------------===//

#include "ETCAGenCallingConv.inc"

SDValue ETCATargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  if (isVarArg)
    report_fatal_error("Vararg functions not supported yet");

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_ETCA);

  for (auto &VA : ArgLocs) {
    MVT ValVT = VA.getValVT();
    const TargetRegisterClass *RC = nullptr;
    if (ValVT == MVT::i64)
      RC = &GPR64RegClass;
    else if (ValVT == MVT::i32)
      RC = &GPR32RegClass;
    else
      RC = &GPRRegClass;

    if (VA.isRegLoc()) {
      Register Reg = MF.addLiveIn(VA.getLocReg(), RC);
      SDValue ArgVal = DAG.getCopyFromReg(Chain, DL, Reg, ValVT);
      InVals.push_back(ArgVal);
    } else {
      unsigned Size = ValVT.getSizeInBits() / 8;
      int FI =
          MF.getFrameInfo().CreateFixedObject(Size, VA.getLocMemOffset(), true);
      SDValue FIN = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
      SDValue Load = DAG.getLoad(ValVT, DL, Chain, FIN,
                                 MachinePointerInfo::getFixedStack(MF, FI));
      InVals.push_back(Load);
    }
  }
  return Chain;
}

SDValue
ETCATargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                bool isVarArg,
                                const SmallVectorImpl<ISD::OutputArg> &Outs,
                                const SmallVectorImpl<SDValue> &OutVals,
                                const SDLoc &DL, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_ETCA);
  SDValue Glue;
  for (auto &VA : RVLocs) {
    assert(VA.isRegLoc() && "Can only return in registers");
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[VA.getValNo()],
                             Glue);
    Glue = Chain.getValue(1);
  }
  if (Glue.getNode())
    return DAG.getNode(ETCAISD::RET_FLAG, DL, MVT::Other, Chain, Glue);
  return DAG.getNode(ETCAISD::RET_FLAG, DL, MVT::Other, Chain);
}

SDValue ETCATargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                      SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  const SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  const SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  const SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool isVarArg = CLI.IsVarArg;
  MachineFunction &MF = DAG.getMachineFunction();
  if (isVarArg)
    report_fatal_error("Vararg functions not supported yet");

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_ETCA);

  SmallVector<std::pair<unsigned, SDValue>, 8> RegsToPass;
  SDValue StackPtr;
  SmallVector<SDValue, 8> MemOpChains;

  MVT PtrVT = getPointerTy(DAG.getDataLayout());

  for (unsigned i = 0; i != ArgLocs.size(); ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];
    if (VA.isRegLoc()) {
      RegsToPass.push_back({VA.getLocReg(), Arg});
    } else {
      assert(VA.isMemLoc());
      if (!StackPtr)
        StackPtr = DAG.getCopyFromReg(Chain, DL, R6, PtrVT);
      SDValue PtrOff =
          DAG.getNode(ISD::ADD, DL, PtrVT, StackPtr,
                      DAG.getConstant(VA.getLocMemOffset(), DL, PtrVT));
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, Arg, PtrOff, MachinePointerInfo()));
    }
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  SDValue Glue;
  for (auto &Reg : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg.first, Reg.second, Glue);
    Glue = Chain.getValue(1);
  }

  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, PtrVT);
  else if (auto *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), PtrVT);

  SmallVector<SDValue, 8> Ops = {Chain, Callee};
  for (auto &Reg : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg.first, Reg.second.getValueType()));
  if (Glue.getNode())
    Ops.push_back(Glue);

  Chain =
      DAG.getNode(ETCAISD::CALL, DL, DAG.getVTList(MVT::Other, MVT::Glue), Ops);

  SmallVector<CCValAssign, 16> RVLocs;
  CCState RCCInfo(CallConv, isVarArg, MF, RVLocs, *DAG.getContext());
  RCCInfo.AnalyzeCallResult(Ins, RetCC_ETCA);
  Glue = Chain.getValue(1);
  for (auto &VA : RVLocs) {
    Chain = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getValVT(), Glue);
    Glue = Chain.getValue(1);
    InVals.push_back(Chain.getValue(0));
  }
  return Chain;
}

bool ETCATargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                               const AddrMode &AM, Type *Ty,
                                               unsigned AS,
                                               Instruction *I) const {
  return AM.BaseGV == nullptr && AM.HasBaseReg && AM.Scale == 0 &&
         AM.BaseOffs == 0;
}
