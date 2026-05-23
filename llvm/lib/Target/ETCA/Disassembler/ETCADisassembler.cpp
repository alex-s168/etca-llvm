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
// Uses the tablegen-generated decoder table (ETCAGenDisassemblerTables.inc)
// for core instruction decoding.  A manual wrapper handles:
//   - REX prefix (0xC0-0xCF) for extended registers (r8-r15, d8-d15, q8-q15)
//
// Instruction formats (16-bit value, bits numbered 15..0):
//   RR:   rA[15:13] | rB[12:10] | 00[9:8] | 00[7:6] | SS[5:4] | CCCC[3:0]
//   RI:   rA[15:13] | imm[12:8]  | 01[7:6] | SS[5:4] | CCCC[3:0]
//   BR:   1|0|D8[13]|CCCC[12:8] | D[7:0]
//   SAF_JMP: bytes 0xAF, 0x(RRR[7:5]|X[4]|CCCC[3:0])
//   SAF_CALL: bytes 0xB0|D[11:8], 0xD[7:0]
//
// REX prefix (0xC0-0xCF): 1100 Q A B X
//   A = bit 3 of AAA register field (bits [15:13])
//   B = bit 3 of BBB register field (bits [12:10])
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/ETCATargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_OPERAND_TYPE
#include "ETCAGenInstrInfo.inc"

#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"

#define DEBUG_TYPE "etca-disassembler"

using namespace llvm;
using namespace llvm::MCD;

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {

class ETCADisassembler : public MCDisassembler {
public:
  ETCADisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx), RexFlags(0) {}

  ~ETCADisassembler() override = default;

  /// REX prefix flags, stored here so that the tablegen-generated decoder
  /// functions can access them through the MCDisassembler pointer.
  ///   bit 2 = REX.A (extends AAA register field, bits [15:13])
  ///   bit 1 = REX.B (extends BBB register field, bits [12:10])
  mutable unsigned RexFlags;

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};

// ---------------------------------------------------------------------------
// REX-aware register decoder helpers
//
// These functions are called by the tablegen-generated decodeToMCInst().
// They must be defined BEFORE the include of ETCAGenDisassemblerTables.inc
// so that the template instantiation can find them.
//
// They use the RexFlags stored in ETCADisassembler to extend 3-bit register
// fields to 4-bit register numbers when a REX prefix is present.
//
// The ambiguity between AAA field (bits [15:13], extended by REX.A) and BBB
// field (bits [12:10], extended by REX.B) is resolved by examining the
// instruction opcode and position in the operand list:
//   - 1st register operand -> AAA -> REX.A  (except PUSH which uses BBB)
//   - 2nd register operand, tied instruction -> AAA (same as 1st) -> REX.A
//   - 2nd register operand, untied         -> BBB -> REX.B
//   - 3rd register operand                  -> BBB -> REX.B
// ---------------------------------------------------------------------------

/// Return true if the given opcode has a tied $src1 = $dst constraint.
static bool hasTiedOperands(unsigned Opc) {
  switch (Opc) {
  // --- 8-bit tied RR ---
  case ETCA::ADD8:
  case ETCA::SUB8:
  case ETCA::RSUB8:
  case ETCA::OR8:
  case ETCA::XOR8:
  case ETCA::AND8:
  // --- 16-bit tied RR ---
  case ETCA::ADD16:
  case ETCA::SUB16:
  case ETCA::RSUB16:
  case ETCA::OR16:
  case ETCA::XOR16:
  case ETCA::AND16:
  // --- 32-bit tied RR ---
  case ETCA::ADD32:
  case ETCA::SUB32:
  case ETCA::RSUB32:
  case ETCA::OR32:
  case ETCA::XOR32:
  case ETCA::AND32:
  // --- 64-bit tied RR ---
  case ETCA::ADD64:
  case ETCA::SUB64:
  case ETCA::RSUB64:
  case ETCA::OR64:
  case ETCA::XOR64:
  case ETCA::AND64:
  // --- 8-bit tied RI ---
  case ETCA::ADDI8:
  case ETCA::SUBI8:
  case ETCA::RSUBI8:
  case ETCA::CMPI8:
  case ETCA::ORI8:
  case ETCA::XORI8:
  case ETCA::ANDI8:
  case ETCA::TESTI8:
  // --- 16-bit tied RI ---
  case ETCA::ADDI16:
  case ETCA::SUBI16:
  case ETCA::RSUBI16:
  case ETCA::CMPI16:
  case ETCA::ORI16:
  case ETCA::XORI16:
  case ETCA::ANDI16:
  case ETCA::TESTI16:
  // --- 32-bit tied RI ---
  case ETCA::ADDI32:
  case ETCA::SUBI32:
  case ETCA::RSUBI32:
  case ETCA::CMPI32:
  case ETCA::ORI32:
  case ETCA::XORI32:
  case ETCA::ANDI32:
  case ETCA::TESTI32:
  // --- 64-bit tied RI ---
  case ETCA::ADDI64:
  case ETCA::SUBI64:
  case ETCA::RSUBI64:
  case ETCA::CMPI64:
  case ETCA::ORI64:
  case ETCA::XORI64:
  case ETCA::ANDI64:
  case ETCA::TESTI64:
  // --- CR access (tied RI) ---
  case ETCA::READCR:
  case ETCA::WRITECR:
    return true;
  default:
    return false;
  }
}

/// Determine which REX bit to use based on field position and opcode.
/// Returns 2 for REX.A (AAA field) or 1 for REX.B (BBB field).
static unsigned getRexBitForOperand(const MCInst &MI, unsigned Opc) {
  unsigned NumRegOps = MI.getNumOperands();
  if (NumRegOps == 0) {
    // First register operand: normally AAA field (REX.A), except PUSH
    // which stores the source register in BBB field.
    switch (Opc) {
    case ETCA::PUSH8:
    case ETCA::PUSH:
    case ETCA::PUSH32:
    case ETCA::PUSH64:
      return 1; // REX.B
    default:
      return 2; // REX.A
    }
  }
  // Second register operand: tied instructions duplicate AAA (REX.A),
  // untied instructions use BBB (REX.B).
  // Third+ register operands: always BBB (REX.B).
  if (NumRegOps == 1 && hasTiedOperands(Opc))
    return 2; // REX.A (tied replica of AAA)
  return 1;   // REX.B
}

// --- Register decoder: 16-bit GPR (r0-r15) ---
static DecodeStatus DecodeGPRRegisterClass(MCInst &MI, unsigned RegNo,
                                           uint64_t Address,
                                           const MCDisassembler *Decoder) {
  const auto &ETCAD = *static_cast<const ETCADisassembler *>(Decoder);
  unsigned RexFlags = ETCAD.RexFlags;

  if (RexFlags) {
    unsigned RexBit = getRexBitForOperand(MI, MI.getOpcode());
    RegNo |= (((RexFlags >> RexBit) & 1) << 3);
  }

  if (RegNo >= 16)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createReg(ETCA::R0 + RegNo));
  return MCDisassembler::Success;
}

// --- Register decoder: 32-bit GPR (d0-d15) ---
static DecodeStatus DecodeGPR32RegisterClass(MCInst &MI, unsigned RegNo,
                                             uint64_t Address,
                                             const MCDisassembler *Decoder) {
  const auto &ETCAD = *static_cast<const ETCADisassembler *>(Decoder);
  unsigned RexFlags = ETCAD.RexFlags;

  if (RexFlags) {
    unsigned RexBit = getRexBitForOperand(MI, MI.getOpcode());
    RegNo |= (((RexFlags >> RexBit) & 1) << 3);
  }

  if (RegNo >= 16)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createReg(ETCA::D0 + RegNo));
  return MCDisassembler::Success;
}

// --- Register decoder: 64-bit GPR (q0-q15) ---
static DecodeStatus DecodeGPR64RegisterClass(MCInst &MI, unsigned RegNo,
                                             uint64_t Address,
                                             const MCDisassembler *Decoder) {
  const auto &ETCAD = *static_cast<const ETCADisassembler *>(Decoder);
  unsigned RexFlags = ETCAD.RexFlags;

  if (RexFlags) {
    unsigned RexBit = getRexBitForOperand(MI, MI.getOpcode());
    RegNo |= (((RexFlags >> RexBit) & 1) << 3);
  }

  if (RegNo >= 16)
    return MCDisassembler::Fail;
  MI.addOperand(MCOperand::createReg(ETCA::Q0 + RegNo));
  return MCDisassembler::Success;
}

// --- Branch target decoder: 9-bit signed displacement, scaled by 2 ---
// The encoder divides byte offset by 2 (instruction units), so the decoder
// must multiply by 2 to recover the byte offset.
static DecodeStatus decodeBranchTarget(MCInst &MI, unsigned Imm,
                                       uint64_t Address,
                                       const MCDisassembler *Decoder) {
  // Sign-extend 9-bit value
  int16_t Disp = Imm & 0x1FF;
  if (Disp & 0x100)
    Disp |= 0xFE00;
  // Scale by 2 (instruction units -> bytes)
  Disp = Disp * 2;
  MI.addOperand(MCOperand::createImm(Disp));
  return MCDisassembler::Success;
}

// --- SAF call target decoder: 12-bit signed displacement, scaled by 2 ---
static DecodeStatus decodeCallTarget(MCInst &MI, unsigned Imm, uint64_t Address,
                                     const MCDisassembler *Decoder) {
  // Sign-extend 12-bit value
  int16_t Disp = Imm & 0xFFF;
  if (Disp & 0x800)
    Disp |= 0xF000;
  // Scale by 2 (instruction units -> bytes)
  Disp = Disp * 2;
  MI.addOperand(MCOperand::createImm(Disp));
  return MCDisassembler::Success;
}

} // end anonymous namespace

// Include the tablegen-generated decoder tables.
// The anonymous namespace functions above (DecodeGPRRegisterClass, etc.) are
// called by the generated decodeToMCInst<> template.
#include "ETCAGenDisassemblerTables.inc"

namespace {

// ---------------------------------------------------------------------------
// Main decoding entry point
// ---------------------------------------------------------------------------

DecodeStatus ETCADisassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                              ArrayRef<uint8_t> Bytes,
                                              uint64_t Address,
                                              raw_ostream &CStream) const {
  // Reset REX state.
  RexFlags = 0;
  Size = 0;

  if (Bytes.size() < 2) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  // Read 16-bit instruction word (little-endian byte order).
  uint16_t Insn = Bytes[0] | (uint16_t(Bytes[1]) << 8);
  Size = 2;

  // Check for REX prefix (byte 0xC0-0xCF, i.e., 1100xxxx).
  // When present, it precedes the 2-byte instruction.
  // REX format: 1100 Q A B X
  //   bit 2 (A) = high bit of AAA register field (bits [15:13])
  //   bit 1 (B) = high bit of BBB register field (bits [12:10])
  if (Bytes.size() >= 3 && (Bytes[0] & 0xF0) == 0xC0) {
    RexFlags = Bytes[0] & 0x0F;
    Insn = Bytes[1] | (uint16_t(Bytes[2]) << 8);
    Size = 3;
  }

  // Call the tablegen-generated decoder.  It handles all instruction formats
  // and calls our custom DecodeGPR{,.32,.64}RegisterClass which apply REX
  // extensions when RexFlags is non-zero.
  Instr.clear();
  if (decodeInstruction(DecoderTable16, Instr, Insn, Address, this, STI))
    return MCDisassembler::Success;

  Size = 0;
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
