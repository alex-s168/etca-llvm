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
//   ALU ops (ADD, SUB, AND, OR, XOR) and ADDI/SUBI use a tied-def constraint
//   ($src1 = $dst) because the hardware encodes dest as the same register
//   as the first source.  The instruction selector emits the tied-def
//   directly (using $src1 as the tied source operand).  The
//   TwoAddressInstructionPass then converts to non-SSA form by inserting a
//   COPY of $src1 to $dst when $dst != $src1.  This is the conventional
//   approach — no need for the instruction selector to pre-insert copies.
//
//   MOVZ, MOVS, MOVZI, MOVSI, READCR, WRITECR do NOT have tied-defs
//   because they ignore the old dest value.  They use the _NT (non-tied)
//   instruction format.
//
//===----------------------------------------------------------------------===//

#include "ETCAInstructionSelector.h"
#include "ETCARegisterInfo.h"
#include "ETCASubtarget.h"
#include "ETCATargetMachine.h"
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
    if (Size == 64)
      return ETCA::ADD64;
    if (Size == 32)
      return ETCA::ADD32;
    if (Size == 8)
      return ETCA::ADD8;
    return ETCA::ADD16;
  case TargetOpcode::G_SUB:
    if (Size == 64)
      return ETCA::SUB64;
    if (Size == 32)
      return ETCA::SUB32;
    if (Size == 8)
      return ETCA::SUB8;
    return ETCA::SUB16;
  case TargetOpcode::G_AND:
    if (Size == 64)
      return ETCA::AND64;
    if (Size == 32)
      return ETCA::AND32;
    if (Size == 8)
      return ETCA::AND8;
    return ETCA::AND16;
  case TargetOpcode::G_OR:
    if (Size == 64)
      return ETCA::OR64;
    if (Size == 32)
      return ETCA::OR32;
    if (Size == 8)
      return ETCA::OR8;
    return ETCA::OR16;
  case TargetOpcode::G_XOR:
    if (Size == 64)
      return ETCA::XOR64;
    if (Size == 32)
      return ETCA::XOR32;
    if (Size == 8)
      return ETCA::XOR8;
    return ETCA::XOR16;
  case TargetOpcode::G_SHL:
    return ETCA::SLO16; // SLO is always 16-bit in base ISA
  default:
    return 0;
  }
}

// Map a size to the MOVZ for that width
static unsigned getMovzOpc(unsigned Size) {
  if (Size == 64)
    return ETCA::MOVZ64;
  if (Size == 32)
    return ETCA::MOVZ32;
  if (Size == 8)
    return ETCA::MOVZ8;
  return ETCA::MOVZ16;
}

// Map a size to the MOVZI (immediate) for that width
static unsigned getMovziOpc(unsigned Size) {
  if (Size == 64)
    return ETCA::MOVZI64;
  if (Size == 32)
    return ETCA::MOVZI32;
  if (Size == 8)
    return ETCA::MOVZI8;
  return ETCA::MOVZI16;
}

/// Return the register class for a given LLT type.
static const TargetRegisterClass *getRCForType(LLT Ty) {
  unsigned Size = Ty.getSizeInBits();
  if (Size == 64)
    return &ETCA::GPR64RegClass;
  if (Size == 32)
    return &ETCA::GPR32RegClass;
  return &ETCA::GPRRegClass; // s8, s16, or pointer types with size < 32
}

/// Constrain a virtual register to the appropriate register class.
static bool constrainReg(Register Reg, LLT Ty, const RegisterBankInfo &RBI,
                         MachineRegisterInfo &MRI) {
  const TargetRegisterClass *RC = getRCForType(Ty);
  return RBI.constrainGenericRegister(Reg, *RC, MRI);
}

// Encode a CmpInst::Predicate into an ICMP_Pseudo pred value (1-10).
// These values match the encoding used by SELECT_Pseudo and ETCASelectExpand.
static int64_t encodeICMPPred(CmpInst::Predicate P) {
  switch (P) {
  case CmpInst::ICMP_EQ:
    return 1;
  case CmpInst::ICMP_NE:
    return 2;
  case CmpInst::ICMP_UGT:
    return 3;
  case CmpInst::ICMP_UGE:
    return 4;
  case CmpInst::ICMP_ULT:
    return 5;
  case CmpInst::ICMP_ULE:
    return 6;
  case CmpInst::ICMP_SGT:
    return 7;
  case CmpInst::ICMP_SGE:
    return 8;
  case CmpInst::ICMP_SLT:
    return 9;
  case CmpInst::ICMP_SLE:
    return 10;
  default:
    return 2; // NE (conservative fallback)
  }
}

// Map a size in bits to an ETCA CMP opcode.
static unsigned getCmpOpcForSize(unsigned Size) {
  if (Size == 64)
    return ETCA::CMP64;
  if (Size == 32)
    return ETCA::CMP32;
  if (Size == 8)
    return ETCA::CMP8;
  return ETCA::CMP; // 16-bit
}

// Map an ICMP_Pseudo pred value (1-10) to a branch opcode.
static unsigned getBranchOpcForPred(int64_t Pred) {
  switch (Pred) {
  case 1:
    return ETCA::BEQ;
  case 2:
    return ETCA::BNE;
  case 3:
    return ETCA::BGTU;
  case 4:
    return ETCA::BGEU;
  case 5:
    return ETCA::BLTU;
  case 6:
    return ETCA::BLEU;
  case 7:
    return ETCA::BGT;
  case 8:
    return ETCA::BGE;
  case 9:
    return ETCA::BLT;
  case 10:
    return ETCA::BLE;
  default:
    return ETCA::BNE;
  }
}

ETCAInstructionSelector::ETCAInstructionSelector(const ETCASubtarget &ST,
                                                 const RegisterBankInfo &RBI)
    : InstructionSelector(), ST(ST), RBI(RBI), TRI(*ST.getRegisterInfo()) {}

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
      BuildMI(MBB, MI, MIMD, TII.get(ETCA::MOVSI16), Dst).addImm(Imm & 0x1F);
    } else if (Size == 16 && Val <= 31) {
      BuildMI(MBB, MI, MIMD, TII.get(ETCA::MOVZI16), Dst).addImm(Val);
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
        // Build large constant using MOVZ/SLO chain.
        // SLO16 uses a tied-def format ($dst = $src1), so each SLO16
        // consumes one vreg and produces the next.  A COPY from the
        // last chain vreg to Dst satisfies SSA for the G_CONSTANT
        // result.  The PostInstructionSelect COPY eliminator will merge
        // same-class COPYs, which is correct since the chain represents
        // an in-place register transformation.
        Register Tmp = MRI->createVirtualRegister(&ETCA::GPRRegClass);
        BuildMI(MBB, MI, MIMD, TII.get(ETCA::MOVZI16), Tmp)
            .addImm(Chunks.back());
        Chunks.pop_back();
        while (!Chunks.empty()) {
          Register Next = MRI->createVirtualRegister(&ETCA::GPRRegClass);
          BuildMI(MBB, MI, MIMD, TII.get(ETCA::SLO16), Next)
              .addReg(Tmp)
              .addImm(Chunks.back());
          Tmp = Next;
          Chunks.pop_back();
        }
        BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst).addReg(Tmp);
      }
    } else {
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
    // Use MOVZ (RR form) with FrameIndex, NOT MOVZI (immediate form).
    // FrameIndex needs register-based resolution (FrameReg + offset),
    // which eliminateFrameIndex handles by emitting MOVZ+ADDI chain.
    BuildMI(MBB, MI, MIMD, TII.get(getMovzOpc(Size)), Dst).addFrameIndex(FI);
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
    BuildMI(MBB, MI, MIMD, TII.get(getMovziOpc(Size)), Dst).addImm(0);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

    //===----------------------------------------------------------------===//
    // G_INTTOPTR / G_PTRTOINT — pointer-integer conversions
    //
    // These are register-class changes at the MIR level.  G_INTTOPTR converts
    // an integer (sizeof(pointer)) to a pointer; G_PTRTOINT does the reverse.
    // Since the GPR register class is determined by size (not by int/ptr
    // type), both are simple COPY operations.
    //===----------------------------------------------------------------===//

  case TargetOpcode::G_INTTOPTR:
  case TargetOpcode::G_PTRTOINT: {
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    LLT DstTy = MRI->getType(Dst);
    BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst).addReg(Src);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

    //===----------------------------------------------------------------===//
    // Arithmetic and logical operations
    //
    // ALU ops (ADD, SUB, etc.) have tied-def: $src1 = $dst.
    // We emit the tied-def directly.  TwoAddressInstructionPass handles
    // the conversion from SSA form: it sees $dst != $src1 (different
    // vregs) and inserts a COPY of $src1 to $dst before the OP.
    //
    // After TwoAddress pass:
    //   %dst = COPY %src1
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
    unsigned Size = DstTy.getSizeInBits();

    // For ADD and SUB, check if Src2 is a small constant that fits in
    // the RI immediate range ([-16, 15] for 16-bit, [-16, 15] for 32/64).
    // If so, use ADDI/SUBI to avoid allocating a register for the
    // constant and thus avoid unnecessary callee-saved-register spills.
    bool UseImmForm = false;
    int64_t ImmVal = 0;
    if (MI.getOpcode() == TargetOpcode::G_ADD ||
        MI.getOpcode() == TargetOpcode::G_SUB) {
      if (auto *DefMI = MRI->getVRegDef(Src2)) {
        if (DefMI->getOpcode() == TargetOpcode::G_CONSTANT) {
          ImmVal = DefMI->getOperand(1).getCImm()->getSExtValue();
          // ETCa RI range: [-16, 15] for all word sizes.
          if (ImmVal >= -16 && ImmVal <= 15)
            UseImmForm = true;
        }
      }
    }

    if (UseImmForm) {
      // Use ADDI/SUBI (register-immediate form).
      // ADDI dst, src, imm → dst = src + imm
      // SUBI dst, src, imm → dst = src - imm
      unsigned ImmOpc;
      bool IsSub = (MI.getOpcode() == TargetOpcode::G_SUB);
      switch (Size) {
      case 64:
        ImmOpc = IsSub ? ETCA::SUBI64 : ETCA::ADDI64;
        break;
      case 32:
        ImmOpc = IsSub ? ETCA::SUBI32 : ETCA::ADDI32;
        break;
      case 8:
        ImmOpc = IsSub ? ETCA::SUBI8 : ETCA::ADDI8;
        break;
      default:
        ImmOpc = IsSub ? ETCA::SUBI16 : ETCA::ADDI16;
        break;
      }
      // ADDI/SUBI dst, src, imm — not tied (no COPY needed).
      BuildMI(MBB, MI, MIMD, TII.get(ImmOpc), Dst).addReg(Src1).addImm(ImmVal);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
      MI.eraseFromParent();
      return true;
    }

    unsigned ETCaOpc = getETCAAluOpcode(MI.getOpcode(), Size);

    // ALU op with tied-def: $src1 = $dst.  TwoAddressInstructionPass
    // handles the SSA-to-non-SSA conversion (inserts COPY when needed).
    BuildMI(MBB, MI, MIMD, TII.get(ETCaOpc), Dst).addReg(Src1).addReg(Src2);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

    //===----------------------------------------------------------------===//
    // G_SHL — constant shift via SLO (16-bit) or linear ADD (32/64-bit)
    //===----------------------------------------------------------------===//

  case TargetOpcode::G_SHL: {
    Register Dst = MI.getOperand(0).getReg();
    Register Src1 = MI.getOperand(1).getReg();
    Register Src2 = MI.getOperand(2).getReg();
    LLT DstTy = MRI->getType(Dst);
    const TargetRegisterClass *RC = getRCForType(DstTy);
    unsigned Size = DstTy.getSizeInBits();
    unsigned AddOpc = getETCAAluOpcode(TargetOpcode::G_ADD, Size);

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

    // Shift by 0 is a no-op.
    if (ShiftAmt == 0) {
      BuildMI(MBB, MI, MIMD, TII.get(getMovzOpc(Size)), Dst).addReg(Src1);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
      MI.eraseFromParent();
      return true;
    }

    // Poison for shift >= bit width or negative → produce 0 for safety.
    if (ShiftAmt < 0 || ShiftAmt >= (int64_t)Size) {
      BuildMI(MBB, MI, MIMD, TII.get(getMovziOpc(Size)), Dst).addImm(0);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
      MI.eraseFromParent();
      return true;
    }

    // === Shift implementation ===
    //
    // Strategy:
    //   For 16-bit word size, use SLO16 (shift-left-by-5, OR-immediate) for
    //   groups of 5 bits, then ADDs for the remainder.  Each SLO16 with imm=0
    //   is Rd = Rd << 5, equivalent to 5 ADD instructions.
    //
    //   For wider sizes (32/64-bit), fall back to linear ADD decomposition
    //   since SLO16 uses the GPR (16-bit) register class and would produce
    //   a register-class mismatch on wider targets.
    //
    // With SLO: Quotient = N/5 SLOs + Remainder = N%5 ADDs, at most
    // 3+4 = 7 instructions for 16-bit (N=15).
    // Without SLO: N ADDs, at most 63 for 64-bit.

    Register Acc = Src1;

    if (Size == 16) {
      // Use SLO16 for groups-of-5 shifts.
      unsigned Quotient = ShiftAmt / 5;
      unsigned Remainder = ShiftAmt % 5;

      for (unsigned i = 0; i < Quotient; ++i) {
        Register Next = MRI->createVirtualRegister(RC);
        BuildMI(MBB, MI, MIMD, TII.get(ETCA::SLO16), Next)
            .addReg(Acc)
            .addImm(0);
        if (!constrainReg(Next, DstTy, RBI, *MRI))
          return false;
        Acc = Next;
      }

      for (unsigned i = 0; i < Remainder; ++i) {
        Register Next = MRI->createVirtualRegister(RC);
        BuildMI(MBB, MI, MIMD, TII.get(AddOpc), Next).addReg(Acc).addReg(Acc);
        if (!constrainReg(Next, DstTy, RBI, *MRI))
          return false;
        Acc = Next;
      }
    } else {
      // Linear ADD decomposition for 32/64-bit: each ADD doubles = shift by 1.
      for (unsigned i = 0; i < ShiftAmt; ++i) {
        Register Next = MRI->createVirtualRegister(RC);
        BuildMI(MBB, MI, MIMD, TII.get(AddOpc), Next).addReg(Acc).addReg(Acc);
        if (!constrainReg(Next, DstTy, RBI, *MRI))
          return false;
        Acc = Next;
      }
    }

    // Copy Acc to Dst.
    if (Dst != Acc)
      BuildMI(MBB, MI, MIMD, TII.get(getMovzOpc(Size)), Dst).addReg(Acc);

    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
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
    unsigned PtrSize = ST.getPtrSize();
    MachineMemOperand *MMO = *MI.memoperands_begin();
    unsigned Opc;
    // Select the correct load variant based on both data width and
    // pointer width.  When data width != pointer width, we need a
    // mixed-width variant (e.g., LOAD16_P32 for 16-bit data with
    // 32-bit pointer).
    switch (Size) {
    case 8:
      if (PtrSize <= 16)
        Opc = ETCA::LOAD8;
      else if (PtrSize <= 32)
        Opc = ETCA::LOAD8_P32;
      else
        Opc = ETCA::LOAD8_P64;
      break;
    case 32:
      if (PtrSize <= 32)
        Opc = ETCA::LOAD32;
      else
        Opc = ETCA::LOAD32_P64;
      break;
    case 64:
      Opc = ETCA::LOAD64;
      break;
    default: // 16-bit or other
      if (PtrSize <= 16)
        Opc = ETCA::LOAD16;
      else if (PtrSize <= 32)
        Opc = ETCA::LOAD16_P32;
      else
        Opc = ETCA::LOAD16_P64;
      break;
    }
    BuildMI(MBB, MI, MIMD, TII.get(Opc), Dst).addReg(Addr).addMemOperand(MMO);
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
    unsigned PtrSize = ST.getPtrSize();
    MachineMemOperand *MMO = *MI.memoperands_begin();
    unsigned Opc;
    // Select the correct store variant based on both data width and
    // pointer width.  When data width != pointer width, we need a
    // mixed-width variant (e.g., STORE16_P32 for 16-bit data with
    // 32-bit pointer).
    switch (Size) {
    case 8:
      if (PtrSize <= 16)
        Opc = ETCA::STORE8;
      else if (PtrSize <= 32)
        Opc = ETCA::STORE8_P32;
      else
        Opc = ETCA::STORE8_P64;
      break;
    case 32:
      if (PtrSize <= 32)
        Opc = ETCA::STORE32;
      else
        Opc = ETCA::STORE32_P64;
      break;
    case 64:
      Opc = ETCA::STORE64;
      break;
    default: // 16-bit or other
      if (PtrSize <= 16)
        Opc = ETCA::STORE16;
      else if (PtrSize <= 32)
        Opc = ETCA::STORE16_P32;
      else
        Opc = ETCA::STORE16_P64;
      break;
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
    BuildMI(MBB, MI, MIMD, TII.get(ETCA::BR)).addMBB(Target);
    MI.eraseFromParent();
    return true;
  }

    //===----------------------------------------------------------------===//
    // G_BRCOND: conditional branch
    //
    // Three cases:
    //
    // 1. Cond is defined by ICMP_Pseudo (G_ICMP was already selected):
    //    CMP was already emitted by the G_ICMP handler, so just emit the
    //    matching branch using the predicate from ICMP_Pseudo.
    //
    // 2. Cond is defined by G_ICMP (reverse-order traversal:
    //    G_BRCOND is visited before G_ICMP): emit CMP + branch inline,
    //    and replace the G_ICMP with an ICMP_Pseudo so the G_ICMP
    //    handler will skip it when encountered.
    //
    // 3. Otherwise: CMP Cond, 0; BNE Target (free-floating condition).
    //===----------------------------------------------------------------===//

  case TargetOpcode::G_BRCOND: {
    Register Cond = MI.getOperand(0).getReg();
    MachineBasicBlock *Target = MI.getOperand(1).getMBB();

    if (auto *DefMI = MRI->getVRegDef(Cond)) {
      // Case 1: CMP already emitted by G_ICMP handler.
      if (DefMI->getOpcode() == ETCA::ICMP_Pseudo) {
        int64_t Pred = DefMI->getOperand(1).getImm();
        BuildMI(MBB, MI, MIMD, TII.get(getBranchOpcForPred(Pred)))
            .addMBB(Target);
        // Erase the consumed ICMP_Pseudo — its result has no remaining
        // uses after G_BRCOND is removed, and leaving it would cause
        // machine verifier errors about register class mismatches
        // (GPR vs GPR32/GPR64 input operands).
        DefMI->eraseFromParent();
        MI.eraseFromParent();
        return true;
      }

      // Case 2: G_ICMP not yet selected (reverse-order traversal).
      if (DefMI->getOpcode() == TargetOpcode::G_ICMP) {
        CmpInst::Predicate Pred = static_cast<CmpInst::Predicate>(
            DefMI->getOperand(1).getPredicate());
        Register LHS = DefMI->getOperand(2).getReg();
        Register RHS = DefMI->getOperand(3).getReg();
        LLT OpTy = MRI->getType(LHS);
        unsigned OpSize = OpTy.getSizeInBits();

        // Emit width-specific CMP lhs, rhs to set flags.
        BuildMI(MBB, MI, MIMD, TII.get(getCmpOpcForSize(OpSize)))
            .addReg(LHS)
            .addReg(RHS);

        // Emit matching branch.
        int64_t EncPred = encodeICMPPred(Pred);
        BuildMI(MBB, MI, MIMD, TII.get(getBranchOpcForPred(EncPred)))
            .addMBB(Target);

        // Replace G_ICMP with ICMP_Pseudo so the G_ICMP handler is a
        // no-op when encountered later in the traversal.
        Register GICMPDst = DefMI->getOperand(0).getReg();
        BuildMI(MBB, *DefMI, MIMD, TII.get(ETCA::ICMP_Pseudo), GICMPDst)
            .addImm(EncPred);
        // Constrain the ICMP_Pseudo result register to GPR (condition is
        // always 16-bit).  In the normal G_ICMP handler this is done by
        // the explicit constrainReg call, but in this reverse-order path
        // the G_ICMP is erased before its handler ever runs, so we must
        // constrain here.
        if (!constrainReg(GICMPDst, LLT::scalar(16), RBI, *MRI))
          return false;
        DefMI->eraseFromParent();
        MI.eraseFromParent();
        return true;
      }
    }

    // Case 3: Free-floating condition.
    // Emit MOVZI Zero,0; CMP Cond, Zero; BNE Target.
    // Zero must be in GPR (16-bit) register class for CMP compatibility.
    Register Zero = MRI->createVirtualRegister(&ETCA::GPRRegClass);
    BuildMI(MBB, MI, MIMD, TII.get(ETCA::MOVZI16), Zero).addImm(0);
    if (!constrainReg(Zero, LLT::scalar(16), RBI, *MRI))
      return false;
    BuildMI(MBB, MI, MIMD, TII.get(ETCA::CMP)).addReg(Cond).addReg(Zero);
    BuildMI(MBB, MI, MIMD, TII.get(ETCA::BNE)).addMBB(Target);
    MI.eraseFromParent();
    return true;
  }

    //===----------------------------------------------------------------===//
    // G_SELECT: conditional select
    //
    // Emits CMP to set flags, then SELECT_Pseudo which is expanded by the
    // ETCASelectExpand pass into proper branches + MOVZ sequences.
    //
    // Three cases:
    //   1. Cond from ICMP_Pseudo (G_ICMP already selected):
    //      CMP already emitted, extract pred from ICMP_Pseudo.
    //   2. Cond from G_ICMP (reverse-order traversal):
    //      emit CMP, replace G_ICMP with ICMP_Pseudo, extract pred.
    //   3. Otherwise: CMP cond, 0; pred=0 (BNE).
    //===----------------------------------------------------------------===//

  case TargetOpcode::G_SELECT: {
    Register Dst = MI.getOperand(0).getReg();
    Register Cond = MI.getOperand(1).getReg();
    Register TrueVal = MI.getOperand(2).getReg();
    Register FalseVal = MI.getOperand(3).getReg();
    LLT DstTy = MRI->getType(Dst);

    int64_t PredVal = 0;
    bool CMPEmitted = false;

    if (auto *DefMI = MRI->getVRegDef(Cond)) {
      // Case 1: CMP already emitted by G_ICMP handler.
      // ICMP_Pseudo carries the predicate in operand 1 (after its $lhs
      // and $rhs operands were removed).  The ICMP_Pseudo will be erased
      // by ETCASelectExpand (it has no real uses since SELECT_Pseudo
      // only checks the opcode, not the register value).
      if (DefMI->getOpcode() == ETCA::ICMP_Pseudo) {
        PredVal = DefMI->getOperand(1).getImm();
        CMPEmitted = true;
      }
      // Case 2: G_ICMP not yet selected (reverse-order traversal).
      else if (DefMI->getOpcode() == TargetOpcode::G_ICMP) {
        CmpInst::Predicate P = static_cast<CmpInst::Predicate>(
            DefMI->getOperand(1).getPredicate());
        Register LHS = DefMI->getOperand(2).getReg();
        Register RHS = DefMI->getOperand(3).getReg();
        LLT OpTy = MRI->getType(LHS);
        unsigned OpSize = OpTy.getSizeInBits();

        BuildMI(MBB, MI, MIMD, TII.get(getCmpOpcForSize(OpSize)))
            .addReg(LHS)
            .addReg(RHS);
        PredVal = encodeICMPPred(P);
        CMPEmitted = true;

        // Replace G_ICMP with ICMP_Pseudo so the G_ICMP handler is a
        // no-op when encountered later in the traversal.
        Register GICMPDst = DefMI->getOperand(0).getReg();
        BuildMI(MBB, *DefMI, MIMD, TII.get(ETCA::ICMP_Pseudo), GICMPDst)
            .addImm(PredVal);
        // Constrain the ICMP_Pseudo result register to GPR (condition is
        // always 16-bit).  In the normal G_ICMP handler this is done by
        // the explicit constrainReg call, but in this reverse-order path
        // the G_ICMP is erased before its handler ever runs, so we must
        // constrain here.
        if (!constrainReg(GICMPDst, LLT::scalar(16), RBI, *MRI))
          return false;
        DefMI->eraseFromParent();
      }
    }

    if (!CMPEmitted) {
      // Case 3: Free-floating condition.
      // Create Zero in GPR (16-bit) register class for CMP compatibility.
      // CMP always expects GPR operands (the condition is always 16-bit).
      Register Zero = MRI->createVirtualRegister(&ETCA::GPRRegClass);
      BuildMI(MBB, MI, MIMD, TII.get(ETCA::MOVZI16), Zero).addImm(0);
      if (!constrainReg(Zero, LLT::scalar(16), RBI, *MRI))
        return false;
      BuildMI(MBB, MI, MIMD, TII.get(ETCA::CMP)).addReg(Cond).addReg(Zero);
    }

    BuildMI(MBB, MI, MIMD, TII.get(ETCA::SELECT_Pseudo), Dst)
        .addReg(Cond)
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
    unsigned Size = DstTy.getSizeInBits();

    // Check if Src2 is a small constant that fits in RI range.
    // Using ADDI avoids allocating a register for the constant.
    if (auto *DefMI = MRI->getVRegDef(Src2)) {
      if (DefMI->getOpcode() == TargetOpcode::G_CONSTANT) {
        int64_t ImmVal = DefMI->getOperand(1).getCImm()->getSExtValue();
        if (ImmVal >= -16 && ImmVal <= 15) {
          unsigned AddiOpc;
          switch (Size) {
          case 64:
            AddiOpc = ETCA::ADDI64;
            break;
          case 32:
            AddiOpc = ETCA::ADDI32;
            break;
          case 8:
            AddiOpc = ETCA::ADDI8;
            break;
          default:
            AddiOpc = ETCA::ADDI16;
            break;
          }
          // ADDI dst, src, imm — not tied, no COPY needed.
          BuildMI(MBB, MI, MIMD, TII.get(AddiOpc), Dst)
              .addReg(Src1)
              .addImm(ImmVal);
          if (!constrainReg(Dst, DstTy, RBI, *MRI))
            return false;
          MI.eraseFromParent();
          return true;
        }
      }
    }

    // Fall back to RR form (tied-def: $src1 = $dst).
    // TwoAddressInstructionPass handles SSA-to-non-SSA conversion.
    BuildMI(MBB, MI, MIMD, TII.get(getETCAAluOpcode(TargetOpcode::G_ADD, Size)),
            Dst)
        .addReg(Src1)
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
    //   The COPY is handled by copyPhysReg() which emits a register-width
    //   MOVZ.  When tmp and dst map to the same underlying ETCa register
    //   number (e.g., R0 and D0 both encode as register 0), copyPhysReg()
    //   skips the copy as a no-op, eliminating redundant MOVZ instructions.
    //===----------------------------------------------------------------===//

  case TargetOpcode::G_ANYEXT: {
    // G_ANYEXT is a zero-cost extension — high bits are undefined.
    // Just emit a COPY to bridge register classes.
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    LLT DstTy = MRI->getType(Dst);
    LLT SrcTy = MRI->getType(Src);
    if (DstTy == SrcTy) {
      // Same register class: direct COPY (no extension needed).
      BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst).addReg(Src);
    } else {
      // Different register classes (but same underlying register) or
      // same class, different size: just COPY to bridge.
      const TargetRegisterClass *SrcRC = getRCForType(SrcTy);
      Register Tmp = MRI->createVirtualRegister(SrcRC);
      BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Tmp).addReg(Src);
      if (!constrainReg(Tmp, SrcTy, RBI, *MRI))
        return false;
      BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst).addReg(Tmp);
    }
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

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
      BuildMI(MBB, MI, MIMD, TII.get(Opc), Dst).addReg(Src);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
    } else {
      // Different widths: use narrow MOVZ/MOVS for correct extension
      // semantics, then COPY to bridge register classes.
      const TargetRegisterClass *SrcRC = getRCForType(SrcTy);
      Register Tmp = MRI->createVirtualRegister(SrcRC);
      BuildMI(MBB, MI, MIMD, TII.get(Opc), Tmp).addReg(Src);
      if (!constrainReg(Tmp, SrcTy, RBI, *MRI))
        return false;
      BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst).addReg(Tmp);
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
    const TargetRegisterClass *SrcRC = getRCForType(SrcTy);
    const TargetRegisterClass *DstRC = getRCForType(DstTy);

    if (SrcSize == DstSize) {
      // Same width: just copy (no truncation needed).
      BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst).addReg(Src);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
    } else if (SrcRC == DstRC) {
      // Same register class, different width (e.g., s16→s8):
      // use destination-width MOVZ which reads the narrower
      // source width via SS bits and extends to register width.
      BuildMI(MBB, MI, MIMD, TII.get(getMovzOpc(DstSize)), Dst).addReg(Src);
      if (!constrainReg(Dst, DstTy, RBI, *MRI))
        return false;
    } else {
      // Different register classes (e.g., s32→s16, GPR32→GPR):
      // first use source-width MOVZ to get a copy in the source RC,
      // then COPY to bridge to the destination RC.
      Register Tmp = MRI->createVirtualRegister(SrcRC);
      BuildMI(MBB, MI, MIMD, TII.get(getMovzOpc(SrcSize)), Tmp).addReg(Src);
      if (!constrainReg(Tmp, SrcTy, RBI, *MRI))
        return false;
      BuildMI(MBB, MI, MIMD, TII.get(TargetOpcode::COPY), Dst).addReg(Tmp);
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
    // G_ICMP: integer comparison
    //
    // Always emits CMP (sets flags) + ICMP_Pseudo (carries the predicate).
    // G_BRCOND and G_SELECT check for ICMP_Pseudo via MRI and extract the
    // predicate to emit the matching branch or select sequence.
    //
    // ICMP_Pseudo is a non-pre-isel target opcode, so the main select
    // loop skips it.  If the G_ICMP result has no consumers (dead code),
    // the ICMP_Pseudo is still emitted; it will be cleaned up later by
    // dead code elimination.
    //===----------------------------------------------------------------===//

  case TargetOpcode::G_ICMP: {
    Register Dst = MI.getOperand(0).getReg();
    CmpInst::Predicate Pred =
        static_cast<CmpInst::Predicate>(MI.getOperand(1).getPredicate());
    Register LHS = MI.getOperand(2).getReg();
    Register RHS = MI.getOperand(3).getReg();
    LLT OpTy = MRI->getType(LHS);
    unsigned OpSize = OpTy.getSizeInBits();

    // Emit width-specific CMP instruction that sets the flags.
    // Use CMP32/CMP64 for wider operands so the register class matches
    // (CMP16 expects GPR, but 32/64-bit operands are GPR32/GPR64).
    BuildMI(MBB, MI, MIMD, TII.get(getCmpOpcForSize(OpSize)))
        .addReg(LHS)
        .addReg(RHS);

    // Emit ICMP_Pseudo as a marker carrying the predicate.
    // Consumers (G_BRCOND, G_SELECT) check for ICMP_Pseudo via MRI and
    // extract the predicate to emit the matching branch/select sequence.
    // ICMP_Pseudo is a non-pre-isel opcode, so the main select loop
    // will skip it.
    int64_t EncPred = encodeICMPPred(Pred);
    BuildMI(MBB, MI, MIMD, TII.get(ETCA::ICMP_Pseudo), Dst).addImm(EncPred);

    // Constrain the result register (result is always 16-bit condition).
    if (!constrainReg(Dst, LLT::scalar(16), RBI, *MRI))
      return false;
    MI.eraseFromParent();
    return true;
  }

    //===----------------------------------------------------------------===//
    // G_JUMP_TABLE: materialize jump table address
    //
    // Emits JT_Pseudo which carries the jump table index.
    // The AsmPrinter expands JT_Pseudo into a MOVZ referencing the jump
    // table label.
    //===----------------------------------------------------------------===//

  case TargetOpcode::G_JUMP_TABLE: {
    Register Dst = MI.getOperand(0).getReg();
    unsigned JTI = MI.getOperand(1).getIndex();
    LLT DstTy = MRI->getType(Dst);
    BuildMI(MBB, MI, MIMD, TII.get(ETCA::JT_Pseudo), Dst)
        .addJumpTableIndex(JTI);
    if (!constrainReg(Dst, DstTy, RBI, *MRI))
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
      BuildMI(MBB, MI, MIMD, TII.get(ETCA::JMPR)).addReg(Addr);
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
  if (Size == 64)
    return ETCA::MOVS64;
  if (Size == 32)
    return ETCA::MOVS32;
  if (Size == 8)
    return ETCA::MOVS8;
  return ETCA::MOVS16;
}
