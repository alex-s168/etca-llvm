//===-- ETCADisassembler.cpp - Disassembler for ETCA ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ETCA instruction disassembler.
//
// Decodes 16-bit (and REX-extended 3-byte) instructions from raw bytes into
// MCInst structures.  Encoding matches binutils (etca-binutils-gdb).
//
// Instruction formats (16-bit value, bits numbered 15..0):
//   RR:   rA[15:13] | rB[12:10] | 00[9:8] | 00[7:6] | SS[5:4] | CCCC[3:0]
//   RI:   rA[15:13] | imm[12:8]  | 01[7:6] | SS[5:4] | CCCC[3:0]
//   BR:   1|0|D8[13]|CCCC[12:8] | D[7:0]
//   SAF_JMP: bytes 0xAF, 0x(RRR[7:5]|X[4]|CCCC[3:0])
//   SAF_CALL: bytes 0xB0|D[11:8], 0xD[7:0]
//
// When a REX prefix (0xC0-0xCF) precedes the 2-byte instruction, the
// register fields are extended to 4 bits via REX.A (bit 2 of prefix)
// and REX.B (bit 1 of prefix).
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/ETCATargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"

#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"

#define DEBUG_TYPE "etca-disassembler"

using namespace llvm;

namespace {

class ETCADisassembler : public MCDisassembler {
public:
  ETCADisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  ~ETCADisassembler() override = default;

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};

MCDisassembler::DecodeStatus
ETCADisassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                 ArrayRef<uint8_t> Bytes, uint64_t Address,
                                 raw_ostream &CStream) const {
  if (Bytes.size() < 2) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  // Read the 16-bit instruction word (bytes stored as in the object file).
  uint16_t Insn = Bytes[0] | (uint16_t(Bytes[1]) << 8);
  Size = 2;

  // Check for REX prefix (byte with value 0xC0-0xCF, i.e., 1100xxxx).
  // When a REX prefix is present, it precedes the 2-byte instruction.
  // REX format: 1100 Q A B X
  //   A = bit 3 of AAA register field (bits [15:13])
  //   B = bit 3 of BBB register field (bits [12:10])
  unsigned RexFlags = 0;
  if (Bytes.size() >= 3 && (Bytes[0] & 0xF0) == 0xC0) {
    RexFlags = Bytes[0] & 0x0F;
    Insn = Bytes[1] | (uint16_t(Bytes[2]) << 8);
    Size = 3;
  }

  // REX register helper: given a 3-bit register field, return the full
  // 4-bit register number.  REX bit 2 = A, bit 1 = B.
  auto RexReg = [RexFlags](unsigned Field, unsigned RexBit) -> unsigned {
    return Field | (((RexFlags >> RexBit) & 1) << 3);
  };

  // NOP check
  if (Insn == 0x008F) {
    Instr.setOpcode(ETCA::NOP);
    return MCDisassembler::Success;
  }

  unsigned Byte0 = Insn & 0xFF;
  unsigned Byte0Hi2 = (Byte0 >> 6) & 0x3;

  // SAF JMP/CALL (byte0 = 0xAF)
  if (Byte0 == 0xAF) {
    unsigned Byte1 = (Insn >> 8) & 0xFF;
    unsigned Reg = RexReg((Byte1 >> 5) & 0x7, 2); // REX.A
    unsigned X = (Byte1 >> 4) & 0x1;
    unsigned Cond = Byte1 & 0xF;
    if (Cond == 0xE) {
      Instr.setOpcode(X ? ETCA::CALLR : ETCA::JMPR);
      Instr.addOperand(MCOperand::createReg(ETCA::R0 + Reg));
      return MCDisassembler::Success;
    }
    return MCDisassembler::Fail;
  }

  // SAF CALL (byte0 & 0xF0 == 0xB0)
  if ((Byte0 & 0xF0) == 0xB0) {
    int16_t Disp = ((Byte0 & 0xF) << 8) | ((Insn >> 8) & 0xFF);
    if (Disp & 0x800)
      Disp |= 0xF000;
    Disp *= 2;
    Instr.setOpcode(ETCA::CALL);
    Instr.addOperand(MCOperand::createImm(Disp));
    return MCDisassembler::Success;
  }

  // Branch (bits[7:6] = 10)
  if (Byte0Hi2 == 2) {
    unsigned D8 = (Byte0 >> 4) & 1;
    unsigned Cond = Byte0 & 0xF;
    unsigned DispLow = (Insn >> 8) & 0xFF;
    int16_t Disp = (D8 << 8) | DispLow;
    if (Disp & 0x100)
      Disp |= 0xFE00;
    Disp *= 2;

    unsigned Opc;
    switch (Cond) {
    case 0:
      Opc = ETCA::BEQ;
      break;
    case 1:
      Opc = ETCA::BNE;
      break;
    case 2:
      Opc = ETCA::BN;
      break;
    case 3:
      Opc = ETCA::BNN;
      break;
    case 4:
      Opc = ETCA::BLTU;
      break;
    case 5:
      Opc = ETCA::BGEU;
      break;
    case 6:
      Opc = ETCA::BOV;
      break;
    case 7:
      Opc = ETCA::BNOV;
      break;
    case 8:
      Opc = ETCA::BLEU;
      break;
    case 9:
      Opc = ETCA::BGTU;
      break;
    case 10:
      Opc = ETCA::BLT;
      break;
    case 11:
      Opc = ETCA::BGE;
      break;
    case 12:
      Opc = ETCA::BLE;
      break;
    case 13:
      Opc = ETCA::BGT;
      break;
    case 14:
      Opc = ETCA::BR;
      break;
    default:
      return MCDisassembler::Fail;
    }
    Instr.setOpcode(Opc);
    Instr.addOperand(MCOperand::createImm(Disp));
    return MCDisassembler::Success;
  }

  // bits[7:6] = 11 → reserved
  if (Byte0Hi2 == 3)
    return MCDisassembler::Fail;

  unsigned Bits76 = Byte0Hi2;
  unsigned RegA = (Insn >> 13) & 0x7;
  unsigned RegB = (Insn >> 10) & 0x7;
  unsigned SS = (Insn >> 4) & 0x3;
  unsigned CCCC = Insn & 0xF;

  // Select register base based on SS width
  unsigned Base;
  if (SS == 0b10)
    Base = ETCA::D0;
  else if (SS == 0b11)
    Base = ETCA::Q0;
  else
    Base = ETCA::R0;

  unsigned FullA = RexReg(RegA, 2); // REX.A
  unsigned FullB = RexReg(RegB, 1); // REX.B

  if (Bits76 == 0) { // RR format
    // SAF PUSH/POP
    if (CCCC == 0xC) {
      if (RegB != 6)
        return MCDisassembler::Fail;
      if (SS == 0b00) {
        Instr.setOpcode(ETCA::POP8);
        Instr.addOperand(MCOperand::createReg(ETCA::R0 + FullA));
      } else if (SS == 0b10) {
        Instr.setOpcode(ETCA::POP32);
        Instr.addOperand(MCOperand::createReg(ETCA::D0 + FullA));
      } else if (SS == 0b11) {
        Instr.setOpcode(ETCA::POP64);
        Instr.addOperand(MCOperand::createReg(ETCA::Q0 + FullA));
      } else {
        Instr.setOpcode(ETCA::POP);
        Instr.addOperand(MCOperand::createReg(ETCA::R0 + FullA));
      }
      return MCDisassembler::Success;
    }
    if (CCCC == 0xD) {
      if (RegA != 6)
        return MCDisassembler::Fail;
      if (SS == 0b00) {
        Instr.setOpcode(ETCA::PUSH8);
        Instr.addOperand(MCOperand::createReg(ETCA::R0 + FullB));
      } else if (SS == 0b10) {
        Instr.setOpcode(ETCA::PUSH32);
        Instr.addOperand(MCOperand::createReg(ETCA::D0 + FullB));
      } else if (SS == 0b11) {
        Instr.setOpcode(ETCA::PUSH64);
        Instr.addOperand(MCOperand::createReg(ETCA::Q0 + FullB));
      } else {
        Instr.setOpcode(ETCA::PUSH);
        Instr.addOperand(MCOperand::createReg(ETCA::R0 + FullB));
      }
      return MCDisassembler::Success;
    }

    unsigned Opc = 0;
    enum OpForm { FormTied3, FormUntied2, FormLoadStore, FormCmpTest };
    OpForm Form = FormTied3;

    if (CCCC <= 9) {
      switch (CCCC) {
      case 0:
        Opc = SS == 0b00   ? ETCA::ADD8
              : SS == 0b10 ? ETCA::ADD32
              : SS == 0b11 ? ETCA::ADD64
                           : ETCA::ADD16;
        break;
      case 1:
        Opc = SS == 0b00   ? ETCA::SUB8
              : SS == 0b10 ? ETCA::SUB32
              : SS == 0b11 ? ETCA::SUB64
                           : ETCA::SUB16;
        break;
      case 2:
        Opc = SS == 0b00   ? ETCA::RSUB8
              : SS == 0b10 ? ETCA::RSUB32
              : SS == 0b11 ? ETCA::RSUB64
                           : ETCA::RSUB16;
        break;
      case 3:
        Opc = SS == 0b00   ? ETCA::CMP8
              : SS == 0b10 ? ETCA::CMP32
              : SS == 0b11 ? ETCA::CMP64
                           : ETCA::CMP;
        Form = FormCmpTest;
        break;
      case 4:
        Opc = SS == 0b00   ? ETCA::OR8
              : SS == 0b10 ? ETCA::OR32
              : SS == 0b11 ? ETCA::OR64
                           : ETCA::OR16;
        break;
      case 5:
        Opc = SS == 0b00   ? ETCA::XOR8
              : SS == 0b10 ? ETCA::XOR32
              : SS == 0b11 ? ETCA::XOR64
                           : ETCA::XOR16;
        break;
      case 6:
        Opc = SS == 0b00   ? ETCA::AND8
              : SS == 0b10 ? ETCA::AND32
              : SS == 0b11 ? ETCA::AND64
                           : ETCA::AND16;
        break;
      case 7:
        Opc = SS == 0b00   ? ETCA::TEST8
              : SS == 0b10 ? ETCA::TEST32
              : SS == 0b11 ? ETCA::TEST64
                           : ETCA::TEST;
        Form = FormCmpTest;
        break;
      case 8:
        Opc = SS == 0b00   ? ETCA::MOVZ8
              : SS == 0b10 ? ETCA::MOVZ32
              : SS == 0b11 ? ETCA::MOVZ64
                           : ETCA::MOVZ16;
        Form = FormUntied2;
        break;
      case 9:
        Opc = SS == 0b00   ? ETCA::MOVS8
              : SS == 0b10 ? ETCA::MOVS32
              : SS == 0b11 ? ETCA::MOVS64
                           : ETCA::MOVS16;
        Form = FormUntied2;
        break;
      }
    } else if (CCCC == 0xA || CCCC == 0xB) {
      Form = FormLoadStore;
      if (CCCC == 0xA)
        Opc = SS == 0b00   ? ETCA::LOAD8
              : SS == 0b10 ? ETCA::LOAD32
              : SS == 0b11 ? ETCA::LOAD64
                           : ETCA::LOAD16;
      else
        Opc = SS == 0b00   ? ETCA::STORE8
              : SS == 0b10 ? ETCA::STORE32
              : SS == 0b11 ? ETCA::STORE64
                           : ETCA::STORE16;
    }

    if (!Opc)
      return MCDisassembler::Fail;
    Instr.setOpcode(Opc);

    switch (Form) {
    case FormLoadStore:
    case FormUntied2:
    case FormCmpTest:
      Instr.addOperand(MCOperand::createReg(Base + FullA));
      Instr.addOperand(MCOperand::createReg(Base + FullB));
      break;
    case FormTied3:
    default:
      Instr.addOperand(MCOperand::createReg(Base + FullA));
      Instr.addOperand(MCOperand::createReg(Base + FullA));
      Instr.addOperand(MCOperand::createReg(Base + FullB));
      break;
    }
    return MCDisassembler::Success;
  }

  if (Bits76 == 1) { // RI format
    unsigned Imm5 = (Insn >> 8) & 0x1F;

    unsigned Opc = 0;
    bool IsNonTied = false;

    // PUSHI: CCCC=13 (0xD), rA=6(sp), imm=Imm5 (must be checked before the
    // switch since CCCC=13 with RegA!=6 is an invalid encoding).
    if (CCCC == 0xD && RegA == 6) {
      if (SS == 0b00)
        Instr.setOpcode(ETCA::PUSHI8);
      else
        Instr.setOpcode(ETCA::PUSHI);
      Instr.addOperand(MCOperand::createImm(Imm5));
      return MCDisassembler::Success;
    }

    switch (CCCC) {
    case 0:
      Opc = SS == 0b00   ? ETCA::ADDI8
            : SS == 0b10 ? ETCA::ADDI32
            : SS == 0b11 ? ETCA::ADDI64
                         : ETCA::ADDI16;
      break;
    case 1:
      Opc = SS == 0b00   ? ETCA::SUBI8
            : SS == 0b10 ? ETCA::SUBI32
            : SS == 0b11 ? ETCA::SUBI64
                         : ETCA::SUBI16;
      break;
    case 2:
      Opc = SS == 0b00   ? ETCA::RSUBI8
            : SS == 0b10 ? ETCA::RSUBI32
            : SS == 0b11 ? ETCA::RSUBI64
                         : ETCA::RSUBI16;
      break;
    case 3:
      Opc = SS == 0b00   ? ETCA::CMPI8
            : SS == 0b10 ? ETCA::CMPI32
            : SS == 0b11 ? ETCA::CMPI64
                         : ETCA::CMPI16;
      break;
    case 4:
      Opc = SS == 0b00   ? ETCA::ORI8
            : SS == 0b10 ? ETCA::ORI32
            : SS == 0b11 ? ETCA::ORI64
                         : ETCA::ORI16;
      break;
    case 5:
      Opc = SS == 0b00   ? ETCA::XORI8
            : SS == 0b10 ? ETCA::XORI32
            : SS == 0b11 ? ETCA::XORI64
                         : ETCA::XORI16;
      break;
    case 6:
      Opc = SS == 0b00   ? ETCA::ANDI8
            : SS == 0b10 ? ETCA::ANDI32
            : SS == 0b11 ? ETCA::ANDI64
                         : ETCA::ANDI16;
      break;
    case 7:
      Opc = SS == 0b00   ? ETCA::TESTI8
            : SS == 0b10 ? ETCA::TESTI32
            : SS == 0b11 ? ETCA::TESTI64
                         : ETCA::TESTI16;
      break;
    case 8:
      Opc = SS == 0b00   ? ETCA::MOVZI8
            : SS == 0b10 ? ETCA::MOVZI32
            : SS == 0b11 ? ETCA::MOVZI64
                         : ETCA::MOVZI16;
      IsNonTied = true;
      break;
    case 9:
      Opc = SS == 0b00   ? ETCA::MOVSI8
            : SS == 0b10 ? ETCA::MOVSI32
            : SS == 0b11 ? ETCA::MOVSI64
                         : ETCA::MOVSI16;
      IsNonTied = true;
      break;
    case 12:
      Opc = ETCA::SLO16;
      IsNonTied = true;
      break;
    // CCCC=13 (0xD) is intentionally absent — PUSHI handled above, otherwise invalid.
    case 14:
      Opc = ETCA::READCR;
      break;
    case 15:
      Opc = ETCA::WRITECR;
      break;
    }

    if (!Opc)
      return MCDisassembler::Fail;

    Instr.setOpcode(Opc);

    if (IsNonTied) {
      Instr.addOperand(MCOperand::createReg(Base + FullA));
      Instr.addOperand(MCOperand::createImm(Imm5));
    } else {
      Instr.addOperand(MCOperand::createReg(Base + FullA));
      Instr.addOperand(MCOperand::createReg(Base + FullA));
      Instr.addOperand(MCOperand::createImm(Imm5));
    }
    return MCDisassembler::Success;
  }

  return MCDisassembler::Fail;
}

} // end anonymous namespace

static MCDisassembler *createETCADisassembler(const Target &T,
                                              const MCSubtargetInfo &STI,
                                              MCContext &Ctx) {
  return new ETCADisassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeETCADisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheETCATarget(),
                                         createETCADisassembler);
}
