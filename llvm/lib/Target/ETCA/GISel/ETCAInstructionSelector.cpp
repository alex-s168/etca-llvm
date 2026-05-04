//===- ETCAInstructionSelector.cpp - ETCA Instruction Selector ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// GlobalISel instruction selector for ETCA.
//
// Maps generic MachineInstrs to ETCA-specific target instructions.
// Handles all word sizes (16, 32, 64) and SAF extension instructions.
//
// NOTE on two-address instructions:
//   ALU ops (ADD, SUB, AND, OR, XOR) and CMPI use a tied-def constraint
//   ($src1 = $dst) because the hardware encodes dest as the same register
//   as the first source.  The instruction selector uses a TEMPORARY vreg
//   for the first source operand, which DIFFERS from the destination.
//   The TwoAddressInstructionPass then inserts a COPY to satisfy the
//   constraint.  This keeps the MIR in SSA form (each vreg defined once)
//   before TwoAddress passes.
//
//   MOVZ, MOVS, MOVZI, MOVSI, READCR, WRITECR do NOT have tied-defs
//   because they ignore the old dest value.  They use the _NT (non-tied)
//   instruction format.
//
//===----------------------------------------------------------------------===//

#include "ETCAInstructionSelector.h"
#include "ETCASubtarget.h"
#include "ETCATargetMachine.h"
#include "ETCARegisterInfo.h"
#include "llvm/CodeGen/GlobalISel/GenericMachineInstrs.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterBankInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/Debug.h"

#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"

#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"

#define DEBUG_TYPE "etca-isel"

using namespace llvm;

// Map a size in bits to an ETCA ALU opcode family.
static unsigned getETCAAluOpcode(unsigned GOpcode, unsigned Size) {
  switch (GOpcode) {
  case TargetOpcode::G_ADD:
    if (Size == 64) return ETCA::ADD64;
    if (Size == 32) return ETCA::ADD32;
    if (Size == 8)  return ETCA::ADD8;
    return ETCA::ADD16;
  case TargetOpcode::G_SUB:
    if (Size == 64) return ETCA::SUB64;
    if (Size == 32) return ETCA::SUB32;
    if (Size == 8)  return ETCA::SUB8;
    return ETCA::SUB16;
  case TargetOpcode::G_AND:
    if (Size == 64) return ETCA::AND64;
    if (Size == 32) return ETCA::AND32;
    if (Size == 8)  return ETCA::AND8;
    return ETCA::AND16;
  case TargetOpcode::G_OR:
    if (Size == 64) return ETCA::OR64;
    if (Size == 32) return ETCA::OR32;
    if (Size == 8)  return ETCA::OR8;
    return ETCA::OR16;
  case TargetOpcode::G_XOR:
    if (Size == 64) return ETCA::XOR64;
    if (Size == 32) return ETCA::XOR32;
    if (Size == 8)  return ETCA::XOR8;
    return ETCA::XOR16;
  case TargetOpcode::G_SHL:
    return ETCA::SLO16; // SLO is always 16-bit in base ISA
  default:
    return 0;
  }
}

// Map a size to the MOVZ for that width
static unsigned getMovzOpc(unsigned Size) {
  if (Size == 64) return ETCA::MOVZ64;
  if (Size == 32) return ETCA::MOVZ32;
  if (Size == 8)  return ETCA::MOVZ8;
  return ETCA::MOVZ16;
}

// Map a size to the MOVZI (immediate) for that width
static unsigned getMovziOpc(unsigned Size) {
  if (Size == 64) return ETCA::MOVZI64;
  if (Size == 32) return ETCA::MOVZI32;
  if (Size == 8)  return ETCA::MOVZI8;
  return ETCA::MOVZI16;
}

/// Return the register class for a given LLT type.
static const TargetRegisterClass *getRCForType(LLT Ty) {
  if (Ty == LLT::scalar(64)) return &ETCA::GPR64RegClass;
  if (Ty == LLT::scalar(32)) return &ETCA::GPR32RegClass;
  return &ETCA::GPRRegClass; // s8, s16, or pointer types
}

/// Constrain a virtual register to the appropriate register class.
static bool constrainReg(Register Reg, LLT Ty,
                          const RegisterBankInfo &RBI,
                          MachineRegisterInfo &MRI) {
  const TargetRegisterClass *RC = getRCForType(Ty);
  return RBI.constrainGenericRegister(Reg, *RC, MRI);
}

ETCAInstructionSelector::ETCAInstructionSelector(
    const TargetMachine &TM, const ETCASubtarget &ST,
    const RegisterBankInfo &RBI)
    : InstructionSelector(), TM(TM), ST(ST), RBI(RBI),
      TRI(*ST.getRegisterInfo()) {}

void ETCAInstructionSelector::setupGeneratedPerFunctionState(
    MachineFunction &MF) {}

bool ETCAInstructionSelector::select(MachineInstr &MI) {
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo *MRI = &MF.getRegInfo();
  const TargetInstrInfo &TII = *ST.getInstrInfo();
  MachineIRBuilder MIRBuilder(MI);

  // COPY is valid pre and post ISel.  Constrain destination vregs.
  if (MI.getOpcode() == TargetOpcode::COPY) {
    Register Dst = MI.getOperand(0).getReg();
    if (Dst.isVirtual()) {
      LLT DstTy = MRI->getType(Dst);
      if (DstTy.isValid() && !constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
    }
    return true;
  }

  if (!MI.isPreISelOpcode())
    return true;

  MIMetadata MIMD(MI.getDebugLoc());

  switch (MI.getOpcode()) {
  default:
    return false;

  //===----------------------------------------------------------------===//
  // G_CONSTANT: materialize constant into register
  //===----------------------------------------------------------------===//
  //
  // MOVZI is non-tied, format: [dst, imm]
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_CONSTANT: {
    Register Dst = MI.getOperand(0).getReg();
    int64_t Imm = MI.getOperand(1).getCImm()->getSExtValue();
    LLT DstTy = MRI->getType(Dst);
    unsigned Size = DstTy.getSizeInBits();
    uint64_t Mask = (Size == 64) ? UINT64_MAX : ((1ULL << Size) - 1);
    uint64_t Val = static_cast<uint64_t>(Imm) & Mask;

    // For 16-bit constants:
    //   - Negative values that fit in 5-bit signed → single MOVS
    //   - Non-negative values ≤ 31 → single MOVZ
    //   - Larger values → MOVZ + SLO chain
    if (Size == 16 && Imm < 0 && isInt<5>(Imm)) {
      BuildMI(MBB, MI, MIMD, TII.get(ETCA::MOVSI16), Dst)
          .addImm(Imm & 0x1F);
    } else if (Size == 16 && Val <= 31) {
      BuildMI(MBB, MI, MIMD, TII.get(ETCA::MOVZI16), Dst)
          .addImm(Val);
    } else if (Size == 16 && Val > 31) {
      // Large unsigned constant: split into MOVZ + SLO chain.
      SmallVector<unsigned, 4> Chunks;
      uint64_t Tmp = Val;
      while (Tmp) {
        Chunks.push_back(Tmp & 0x1F);
        Tmp >>= 5;
      }
      if (Chunks.empty()) {
        BuildMI(MBB, MI, MIMD, TII.get(ETCA::MOVZI16), Dst).addImm(0);
      } else {
        // First: MOVZ with highest chunk
        BuildMI(MBB, MI, MIMD, TII.get(ETCA::MOVZI16), Dst)
            .addImm(Chunks.back());
        Chunks.pop_back();
        // Remaining chunks: SLO
        while (!Chunks.empty()) {
          BuildMI(MBB, MI, MIMD, TII.get(ETCA::SLO16), Dst)
              .addReg(Dst)
              .addImm(Chunks.back());
          Chunks.pop_back();
        }
      }
    } else {
      // Non-16-bit width: use generic movzi
      BuildMI(MBB, MI, MIMD, TII.get(getMovziOpc(Size)), Dst)
          .addImm(Val & 0x1F);
    }
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_FRAME_INDEX: compute frame pointer + offset
  //===----------------------------------------------------------------===//
  //
  // MOVZI is non-tied, format: [dst, fi]
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_FRAME_INDEX: {
    Register Dst = MI.getOperand(0).getReg();
    int FI = MI.getOperand(1).getIndex();
    LLT DstTy = MRI->getType(Dst);
    unsigned Size = DstTy.getSizeInBits();
    BuildMI(MBB, MI, MIMD, TII.get(getMovziOpc(Size)), Dst)
        .addFrameIndex(FI);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_GLOBAL_VALUE: load address of global
  //===----------------------------------------------------------------===//
  //
  // MOVZI is non-tied, format: [dst, gv]
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_GLOBAL_VALUE: {
    Register Dst = MI.getOperand(0).getReg();
    const GlobalValue *GV = MI.getOperand(1).getGlobal();
    LLT DstTy = MRI->getType(Dst);
    unsigned Size = DstTy.getSizeInBits();
    BuildMI(MBB, MI, MIMD, TII.get(getMovziOpc(Size)), Dst)
        .addGlobalAddress(GV);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_IMPLICIT_DEF
  //===----------------------------------------------------------------===//
  //
  // MOVZI is non-tied, format: [dst, imm]
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_IMPLICIT_DEF: {
    Register Dst = MI.getOperand(0).getReg();
    LLT DstTy = MRI->getType(Dst);
    unsigned Size = DstTy.getSizeInBits();
    BuildMI(MBB, MI, MIMD, TII.get(getMovziOpc(Size)), Dst)
        .addImm(0);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // Arithmetic and logical operations
  //
  // ALU ops (ADD, SUB, etc.) have tied-def: $src1 = $dst.
  // To keep SSA form, we emit:
  //   %tmp = COPY %src1           (source vreg, no tied-def)
  //   %dst = OP %tmp(tied), %src2 (tied src≠dst, TwoAddress inserts COPY)
  //
  // After TwoAddress pass:
  //   %tmp = COPY %src1
  //   %dst = COPY %tmp
  //   %dst = OP %dst(tied), %src2  →  %dst = %src1 OP %src2
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_ADD:
  case TargetOpcode::G_SUB:
  case TargetOpcode::G_AND:
  case TargetOpcode::G_OR:
  case TargetOpcode::G_XOR: {
    Register Dst = MI.getOperand(0).getReg();
    Register Src1 = MI.getOperand(1).getReg();
    Register Src2 = MI.getOperand(2).getReg();
    LLT DstTy = MRI->getType(Dst);
    const TargetRegisterClass *RC = getRCForType(DstTy);
    unsigned Size = DstTy.getSizeInBits();
    unsigned ETCaOpc = getETCAAluOpcode(MI.getOpcode(), Size);

    // Copy Src1 to a temp (no tied-def since COPY is generic).
    Register Tmp = MRI->createVirtualRegister(RC);
    BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Tmp)
        .addReg(Src1);
    if (!constrainReg(Tmp, DstTy, RBI, *MRI))
      return false;

    // ALU op with tied source = Tmp (≠ Dst → TwoAddress inserts COPY).
    BuildMI(MBB, MI, MIMD, TII.get(ETCaOpc), Dst)
        .addReg(Tmp)
        .addReg(Src2);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_SHL — expanded as repeated ADD
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_SHL: {
    Register Dst = MI.getOperand(0).getReg();
    Register Src1 = MI.getOperand(1).getReg();
    Register Src2 = MI.getOperand(2).getReg();
    LLT DstTy = MRI->getType(Dst);
    const TargetRegisterClass *RC = getRCForType(DstTy);
    unsigned Size = DstTy.getSizeInBits();

    // SLO requires an immediate shift amount.
    int64_t ShiftAmt = 0;
    bool HasConst = false;
    if (auto *DefMI = MRI->getVRegDef(Src2)) {
      if (DefMI->getOpcode() == TargetOpcode::G_CONSTANT) {
        ShiftAmt = DefMI->getOperand(1).getCImm()->getSExtValue();
        HasConst = true;
      }
    }
    if (!HasConst)
      return false;

    // Expand as repeated ADD: each iteration doubles via ADD (tied-def).
    // MOVZ initial copy is non-tied.
    unsigned AddOpc = getETCAAluOpcode(TargetOpcode::G_ADD, Size);
    Register Val = ShiftAmt == 0 ? Dst : MRI->createVirtualRegister(RC);
    BuildMI(MBB, MI, MIMD, TII.get(getMovzOpc(Size)), Val)
        .addReg(Src1);
    if (!constrainReg(Val, DstTy, RBI, *MRI))
      return false;

    for (int64_t i = 0; i < ShiftAmt; ++i) {
      Register Next = (i == ShiftAmt - 1) ? Dst : MRI->createVirtualRegister(RC);
      // ADD with tied source = Val (≠ Next) → TwoAddress inserts COPY.
      BuildMI(MBB, MI, MIMD, TII.get(AddOpc), Next)
          .addReg(Val)
          .addReg(Val);
      if (!constrainReg(Next, DstTy, RBI, *MRI))
        return false;
      Val = Next;
    }

    if (ShiftAmt == 0 && Dst != Val) {
      BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst)
          .addReg(Val);
    }
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_LOAD — select width-specific RR-format load
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_LOAD: {
    Register Dst = MI.getOperand(0).getReg();
    Register Addr = MI.getOperand(1).getReg();
    LLT DstTy = MRI->getType(Dst);
    unsigned Size = DstTy.getSizeInBits();
    MachineMemOperand *MMO = *MI.memoperands_begin();
    unsigned Opc;
    switch (Size) {
    case 8:  Opc = ETCA::LOAD8; break;
    case 32: Opc = ETCA::LOAD32; break;
    case 64: Opc = ETCA::LOAD64; break;
    default: Opc = ETCA::LOAD16; break;
    }
    BuildMI(MBB, MI, MIMD, TII.get(Opc), Dst)
        .addReg(Addr)
        .addMemOperand(MMO);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_STORE — select width-specific RR-format store
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_STORE: {
    Register Val = MI.getOperand(0).getReg();
    Register Addr = MI.getOperand(1).getReg();
    LLT ValTy = MRI->getType(Val);
    unsigned Size = ValTy.getSizeInBits();
    MachineMemOperand *MMO = *MI.memoperands_begin();
    unsigned Opc;
    switch (Size) {
    case 8:  Opc = ETCA::STORE8; break;
    case 32: Opc = ETCA::STORE32; break;
    case 64: Opc = ETCA::STORE64; break;
    default: Opc = ETCA::STORE16; break;
    }
    BuildMI(MBB, MI, MIMD, TII.get(Opc))
        .addReg(Val)
        .addReg(Addr)
        .addMemOperand(MMO);
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_BR: unconditional branch
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_BR: {
    MachineBasicBlock *Target = MI.getOperand(0).getMBB();
    BuildMI(MBB, MI, MIMD, TII.get(ETCA::BR))
        .addMBB(Target);
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_BRCOND: conditional branch
  //
  // Two cases:
  //
  // 1. Cond is defined by G_ICMP: use the matching branch for the
  //    predicate, emitting CMP inline with the original operands.
  //
  // 2. Otherwise: CMP Cond, 0; BNE Target.
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_BRCOND: {
    Register Cond = MI.getOperand(0).getReg();
    MachineBasicBlock *Target = MI.getOperand(1).getMBB();
    LLT CondTy = MRI->getType(Cond);
    unsigned Size = CondTy.getSizeInBits();

    // Check if Cond is defined by G_ICMP (may not have been processed yet
    // due to reverse-order iteration — G_BRCOND comes after G_ICMP in
    // reverse, so G_ICMP still exists and hasn't been selected yet).
    if (auto *DefMI = MRI->getVRegDef(Cond)) {
      if (DefMI->getOpcode() == TargetOpcode::G_ICMP) {
        CmpInst::Predicate Pred = static_cast<CmpInst::Predicate>(
            DefMI->getOperand(1).getPredicate());
        Register LHS = DefMI->getOperand(2).getReg();
        Register RHS = DefMI->getOperand(3).getReg();

        // Emit CMP lhs, rhs to set flags.
        BuildMI(MBB, MI, MIMD, TII.get(ETCA::CMP))
            .addReg(LHS)
            .addReg(RHS);

        // Emit matching branch.
        unsigned BrOpc;
        switch (Pred) {
        case CmpInst::ICMP_EQ:  BrOpc = ETCA::BEQ; break;
        case CmpInst::ICMP_NE:  BrOpc = ETCA::BNE; break;
        case CmpInst::ICMP_UGT: BrOpc = ETCA::BGTU; break;
        case CmpInst::ICMP_UGE: BrOpc = ETCA::BGEU; break;
        case CmpInst::ICMP_ULT: BrOpc = ETCA::BLTU; break;
        case CmpInst::ICMP_ULE: BrOpc = ETCA::BLEU; break;
        case CmpInst::ICMP_SGT: BrOpc = ETCA::BGT; break;
        case CmpInst::ICMP_SGE: BrOpc = ETCA::BGE; break;
        case CmpInst::ICMP_SLT: BrOpc = ETCA::BLT; break;
        case CmpInst::ICMP_SLE: BrOpc = ETCA::BLE; break;
        default: BrOpc = ETCA::BNE; break;
        }
        BuildMI(MBB, MI, MIMD, TII.get(BrOpc)).addMBB(Target);

        DefMI->eraseFromParent();
        MI.eraseFromParent();
        return true;
      }
    }

    // No ICMP — emit MOVZI Zero,0; CMP Cond, Zero; BNE Target.
    const TargetRegisterClass *RC = getRCForType(CondTy);
    Register Zero = MRI->createVirtualRegister(RC);
    BuildMI(MBB, MI, MIMD, TII.get(getMovziOpc(Size)), Zero)
        .addImm(0);
    if (!constrainReg(Zero, CondTy, RBI, *MRI))
      return false;
    BuildMI(MBB, MI, MIMD, TII.get(ETCA::CMP))
        .addReg(Cond)
        .addReg(Zero);
    BuildMI(MBB, MI, MIMD, TII.get(ETCA::BNE))
        .addMBB(Target);
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_SELECT: conditional select
  //
  // Emits CMP to set flags, then SELECT_Pseudo which is expanded by the
  // ETCASelectExpand pass into proper branches + MOVZ sequences.
  //
  // Two cases:
  //   1. Cond from G_ICMP: consume G_ICMP, emit CMP with its LHS/RHS,
  //      set pred to encode the matching branch (1=EQ, 2=NE, etc.).
  //   2. Otherwise: CMP cond, 0; pred=0 (BNE).
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_SELECT: {
    Register Dst = MI.getOperand(0).getReg();
    Register Cond = MI.getOperand(1).getReg();
    Register TrueVal = MI.getOperand(2).getReg();
    Register FalseVal = MI.getOperand(3).getReg();
    LLT DstTy = MRI->getType(Dst);
    unsigned Size = DstTy.getSizeInBits();
    const TargetRegisterClass *DstRC = getRCForType(DstTy);

    int64_t PredVal = 0;
    bool IsICMP = false;

    if (auto *DefMI = MRI->getVRegDef(Cond)) {
      if (DefMI->getOpcode() == TargetOpcode::G_ICMP) {
        IsICMP = true;
        CmpInst::Predicate P = static_cast<CmpInst::Predicate>(
            DefMI->getOperand(1).getPredicate());
        Register LHS = DefMI->getOperand(2).getReg();
        Register RHS = DefMI->getOperand(3).getReg();

        BuildMI(MBB, MI, MIMD, TII.get(ETCA::CMP))
            .addReg(LHS).addReg(RHS);

        switch (P) {
        case CmpInst::ICMP_EQ:  PredVal = 1; break;
        case CmpInst::ICMP_NE:  PredVal = 2; break;
        case CmpInst::ICMP_UGT: PredVal = 3; break;
        case CmpInst::ICMP_UGE: PredVal = 4; break;
        case CmpInst::ICMP_ULT: PredVal = 5; break;
        case CmpInst::ICMP_ULE: PredVal = 6; break;
        case CmpInst::ICMP_SGT: PredVal = 7; break;
        case CmpInst::ICMP_SGE: PredVal = 8; break;
        case CmpInst::ICMP_SLT: PredVal = 9; break;
        case CmpInst::ICMP_SLE: PredVal = 10; break;
        default:                PredVal = 1; break;
        }

        DefMI->eraseFromParent();
      }
    }

    if (!IsICMP) {
      Register Zero = MRI->createVirtualRegister(DstRC);
      BuildMI(MBB, MI, MIMD, TII.get(getMovziOpc(Size)), Zero)
          .addImm(0);
      if (!constrainReg(Zero, DstTy, RBI, *MRI))
        return false;
      BuildMI(MBB, MI, MIMD, TII.get(ETCA::CMP))
          .addReg(Cond).addReg(Zero);
    }

    BuildMI(MBB, MI, MIMD, TII.get(ETCA::SELECT_Pseudo), Dst)
        .addReg(Dst)
        .addReg(TrueVal)
        .addReg(FalseVal)
        .addImm(PredVal);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_PTR_ADD
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_PTR_ADD: {
    Register Dst = MI.getOperand(0).getReg();
    Register Src1 = MI.getOperand(1).getReg();
    Register Src2 = MI.getOperand(2).getReg();
    LLT DstTy = MRI->getType(Dst);
    const TargetRegisterClass *RC = getRCForType(DstTy);
    unsigned Size = DstTy.getSizeInBits();

    // Same two-address SSA approach as G_ADD.
    Register Tmp = MRI->createVirtualRegister(RC);
    BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Tmp)
        .addReg(Src1);
    if (!constrainReg(Tmp, DstTy, RBI, *MRI))
      return false;
    BuildMI(MBB, MI, MIMD, TII.get(getETCAAluOpcode(TargetOpcode::G_ADD, Size)), Dst)
        .addReg(Tmp)
        .addReg(Src2);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_ZEXT, G_SEXT
  //
  // MOVZ/MOVS are non-tied, format: [dst, src]
  //
  // IMPORTANT: The MOVZ/MOVS opcode must be selected based on the SOURCE
  // (narrow) width, NOT the destination width.
  //
  // On ETCa hardware, narrow operations (defined by SS bits) sign-extend
  // their results to the full register width.  For example, on a 32-bit
  // machine, ADD16 sign-extends the 16-bit result to fill all 32 bits of
  // the destination register.  The SS bits determine how many bits to
  // READ from the source.  MOVZ16 reads 16 bits and zero-extends to the
  // register width.  MOVZ32 reads 32 bits.
  //
  // If we emit MOVZ32 for a zero-extension from s16 to s32, the hardware
  // reads ALL 32 bits of the source register (including sign-extended high
  // bits from a previous narrow ALU op) and copies them, producing a
  // sign-extended result instead of zero-extended.  Using MOVZ16 (source
  // width = 16) ensures only the correct 16 bits are read and extended.
  //
  // Since MOVZ16 expects GPR operands but we may need a wider destination
  // (GPR32/GPR64), we use a two-step sequence:
  //   1. MOVZ16 tmp(s16), src(s16)   — zero/sign-extend using correct SS
  //   2. COPY dst(s32), tmp(s16)     — bridge register classes
  //   The COPY is handled by copyPhysReg() using register-width MOVZ,
  //   which reads the already-correctly-extended value.
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_ZEXT:
  case TargetOpcode::G_SEXT: {
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    LLT DstTy = MRI->getType(Dst);
    LLT SrcTy = MRI->getType(Src);
    unsigned DstSize = DstTy.getSizeInBits();
    unsigned SrcSize = SrcTy.getSizeInBits();
    unsigned Opc = (MI.getOpcode() == TargetOpcode::G_SEXT)
                       ? getMovsOpc(SrcSize)
                       : getMovzOpc(SrcSize);

    if (SrcSize == DstSize) {
      // Same width: direct MOVZ/MOVS (no extension needed, but
      // G_ZEXT/G_SEXT is a no-op at this width).
      BuildMI(MBB, MI, MIMD, TII.get(Opc), Dst)
          .addReg(Src);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
    } else {
      // Different widths: use narrow MOVZ/MOVS for correct extension
      // semantics, then COPY to bridge register classes.
      const TargetRegisterClass *SrcRC = getRCForType(SrcTy);
      Register Tmp = MRI->createVirtualRegister(SrcRC);
      BuildMI(MBB, MI, MIMD, TII.get(Opc), Tmp)
          .addReg(Src);
      if (!constrainReg(Tmp, SrcTy, RBI, *MRI))
        return false;
      BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst)
          .addReg(Tmp);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
    }
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_TRUNC
  //
  // Uses destination-width MOVZ which reads the narrow width from the
  // source register.  This is correct because MOVZ16 on a wider register
  // reads only the low 16 bits and zero-extends to the register width.
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_TRUNC: {
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    LLT DstTy = MRI->getType(Dst);
    LLT SrcTy = MRI->getType(Src);
    unsigned DstSize = DstTy.getSizeInBits();
    unsigned SrcSize = SrcTy.getSizeInBits();

    if (SrcSize == DstSize) {
      // Same width: just copy (no truncation needed).
      BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst)
          .addReg(Src);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
    } else {
      // Narrow: use destination-width MOVZ which reads the narrower
      // source width via SS bits and extends to register width.
      BuildMI(MBB, MI, MIMD, TII.get(getMovzOpc(DstSize)), Dst)
          .addReg(Src);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
    }
    MI.eraseFromParent();
    return true;
  }



  //===----------------------------------------------------------------===//
  // G_PHI
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_PHI: {
    Register Dst = MI.getOperand(0).getReg();
    LLT DstTy = MRI->getType(Dst);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.setDesc(TII.get(TargetOpcode::PHI));
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_ICMP: integer comparison — emit CMP, erase G_ICMP.
  //
  // G_BRCOND and G_SELECT emit their own CMP instructions inline and
  // do NOT rely on this one (due to reverse-order iteration in the
  // selector: G_SELECT is processed BEFORE G_ICMP, so the CMP emitted
  // here would be placed after the G_SELECT's CMP/branch, which is too
  // late).  Both G_BRCOND and G_SELECT check for the original G_ICMP
  // instruction directly (which still exists since it hasn't been
  // selected yet in reverse order) and consume it themselves.
  //
  // This handler is reached only if the G_ICMP was NOT consumed by
  // G_BRCOND or G_SELECT (i.e., the icmp result is unused).  In that
  // case we still emit CMP for correctness, but it will likely be
  // eliminated as dead code.
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_ICMP: {
    Register Dst = MI.getOperand(0).getReg();
    Register LHS = MI.getOperand(2).getReg();
    Register RHS = MI.getOperand(3).getReg();

    BuildMI(MBB, MI, MIMD, TII.get(ETCA::CMP))
        .addReg(LHS)
        .addReg(RHS);

    // Constrain the result register (even though it's unused).
    if (!constrainReg(Dst, LLT::scalar(16), RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

  //===----------------------------------------------------------------===//
  // G_BRINDIRECT: indirect branch via register (SAF: jmpr)
  //===----------------------------------------------------------------===//

  case TargetOpcode::G_BRINDIRECT: {
    Register Addr = MI.getOperand(0).getReg();
    if (ST.hasSAF()) {
      BuildMI(MBB, MI, MIMD, TII.get(ETCA::JMPR))
          .addReg(Addr);
    } else {
      return false;
    }
    MI.eraseFromParent();
    return true;
  }

  }

  return false;
}

unsigned ETCAInstructionSelector::getMovsOpc(unsigned Size) {
  if (Size == 64) return ETCA::MOVS64;
  if (Size == 32) return ETCA::MOVS32;
  if (Size == 8)  return ETCA::MOVS8;
  return ETCA::MOVS16;
}
