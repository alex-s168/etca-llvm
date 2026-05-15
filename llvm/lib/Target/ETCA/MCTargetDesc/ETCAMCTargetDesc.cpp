//===-- ETCAMCTargetDesc.cpp - ETCA Target Descriptions -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ETCAMCTargetDesc.h"
#include "ETCAFixupKinds.h"
#include "ETCAMCAsmInfo.h"
#include "TargetInfo/ETCATargetInfo.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/SubtargetFeature.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

//===----------------------------------------------------------------------===//
// ETCA MCInstPrinter
//===----------------------------------------------------------------------===//

namespace {
class ETCAInstPrinter : public MCInstPrinter {
public:
  ETCAInstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                  const MCRegisterInfo &MRI)
      : MCInstPrinter(MAI, MII, MRI) {}

  void printRegName(raw_ostream &O, MCRegister Reg) override {
    // Print register with binutils-compatible syntax: %rN, %rNd, %rNq.
    // Use the TableGen-generated name (from getRegisterName) so we stay
    // in sync with the register definitions, but add the '%' prefix
    // required by the etca binutils assembler.
    O << '%' << getRegisterName(Reg);
  }

  void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                 const MCSubtargetInfo &STI, raw_ostream &O) override {
    printInstruction(MI, Address, O);
    printAnnotation(O, Annot);
  }

  void printInstruction(const MCInst *MI, uint64_t Address, raw_ostream &O);
  void printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O);

  std::pair<const char *, uint64_t>
  getMnemonic(const MCInst &MI) const override;

  const char *getRegisterName(MCRegister Reg);
};

} // namespace

#include "ETCAGenAsmWriter.inc"

static MCInstPrinter *createETCAMCInstPrinter(const Triple &T,
                                              unsigned SyntaxVariant,
                                              const MCAsmInfo &MAI,
                                              const MCInstrInfo &MII,
                                              const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new ETCAInstPrinter(MAI, MII, MRI);
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Generated boilerplate
//===----------------------------------------------------------------------===//

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "ETCAGenInstrInfo.inc"

// Return true if the opcode is an 8-bit (byte-width) variant.
static bool isByteOpcode(unsigned Opcode) {
  switch (Opcode) {
  case ETCA::ADD8:
  case ETCA::SUB8:
  case ETCA::RSUB8:
  case ETCA::OR8:
  case ETCA::XOR8:
  case ETCA::AND8:
  case ETCA::MOVZ8:
  case ETCA::MOVS8:
  case ETCA::CMP8:
  case ETCA::TEST8:
  case ETCA::ADDI8:
  case ETCA::SUBI8:
  case ETCA::RSUBI8:
  case ETCA::CMPI8:
  case ETCA::ORI8:
  case ETCA::XORI8:
  case ETCA::ANDI8:
  case ETCA::TESTI8:
  case ETCA::MOVZI8:
  case ETCA::MOVSI8:
  case ETCA::LOAD8:
  case ETCA::STORE8:
    return true;
  default:
    return false;
  }
}

#define GET_SUBTARGETINFO_MC_DESC
#include "ETCAGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "ETCAGenRegisterInfo.inc"

void ETCAInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);
  if (MO.isReg()) {
    unsigned Reg = MO.getReg();
    bool IsRReg = (Reg >= ETCA::R0 && Reg <= ETCA::R7);
    if (IsRReg && isByteOpcode(MI->getOpcode())) {
      // Print 8-bit register with 'h' suffix so the output reassembles
      // as a byte-width instruction: e.g., %r0h instead of %r0.
      O << '%' << getRegisterName(Reg) << 'h';
    } else {
      printRegName(O, Reg);
    }
  } else if (MO.isImm())
    O << MO.getImm();
  else if (MO.isExpr()) {
    MAI.printExpr(O, *MO.getExpr());
  } else
    O << "<?>";
}

static MCInstrInfo *createETCAMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitETCAMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createETCAMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitETCAMCRegisterInfo(X, /*ReturnAddressReg=*/7);
  return X;
}

static MCSubtargetInfo *createETCAMCSubtargetInfo(const Triple &TT,
                                                  StringRef CPU, StringRef FS) {
  return createETCAMCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

static MCAsmInfo *createETCAMCAsmInfo(const MCRegisterInfo &, const Triple &TT,
                                      const MCTargetOptions &Options) {
  return new ETCAMCAsmInfo(TT, Options);
}

//===----------------------------------------------------------------------===//
// ETCA MCCodeEmitter
//===----------------------------------------------------------------------===//
//
// ETCA instruction encoding is fully manual because the TableGen-generated
// emitter (ETCAGenMCCodeEmitter.inc) does not match the actual instruction
// format bit assignments defined in ETCAInstrFormats.td.
//
// All instructions are 16-bit fixed width, little-endian.
//
//===----------------------------------------------------------------------===//

// NOTE: We do NOT include ETCAGenMCCodeEmitter.inc because the
// TableGen-generated encoder uses incorrect bit shifts that don't match
// ETCAInstrFormats.td. Instead, we use the fully manual encoder in the
// ETCAMCCodeEmitter class below.

namespace {

class ETCAMCCodeEmitter : public MCCodeEmitter {
  const MCInstrInfo &MCII;

public:
  ETCAMCCodeEmitter(const MCInstrInfo &MII, MCContext &Ctx) : MCII(MII) {
    (void)MCII;
  }

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

private:
  /// Return the register number (0-7) from a register operand.
  unsigned getRegisterOpValue(const MCInst &MI, unsigned OpNo) const;

  /// Return the immediate value from an operand, or 0 if it's an expression.
  /// If the operand is an MCExpr, records a fixup and returns 0.
  unsigned getImmOpValue(const MCInst &MI, unsigned OpNo,
                         SmallVectorImpl<MCFixup> &Fixups,
                         MCFixupKind FixupKind) const;

  /// Branch target encoding: 9-bit signed displacement.
  /// Returns the encoded bits (high bit in bit 8, low 8 bits in bits 7:0).
  unsigned encodeBranchTarget(const MCInst &MI, unsigned OpNo,
                              SmallVectorImpl<MCFixup> &Fixups) const;

  /// CALL target encoding: 12-bit signed displacement.
  unsigned encodeCallTarget(const MCInst &MI, unsigned OpNo,
                            SmallVectorImpl<MCFixup> &Fixups) const;
};

} // namespace

unsigned ETCAMCCodeEmitter::getRegisterOpValue(const MCInst &MI,
                                               unsigned OpNo) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  assert(MO.isReg() && "Expected register operand");
  // Register encoding: extract the 3-bit index (0-7) for each register class.
  unsigned Reg = MO.getReg();
  if (Reg >= ETCA::R0 && Reg <= ETCA::R7)
    return Reg - ETCA::R0;
  if (Reg >= ETCA::D0 && Reg <= ETCA::D7)
    return Reg - ETCA::D0;
  if (Reg >= ETCA::Q0 && Reg <= ETCA::Q7)
    return Reg - ETCA::Q0;
  llvm_unreachable("Unknown ETCA register in encoder");
}

unsigned ETCAMCCodeEmitter::getImmOpValue(const MCInst &MI, unsigned OpNo,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          MCFixupKind FixupKind) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  if (MO.isExpr()) {
    Fixups.push_back(MCFixup::create(0, MO.getExpr(), FixupKind));
    return 0;
  }
  return 0;
}

unsigned
ETCAMCCodeEmitter::encodeBranchTarget(const MCInst &MI, unsigned OpNo,
                                      SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm()) {
    // Immediate branch displacement already resolved.
    // Value is the byte offset from PC.
    int64_t Offset = MO.getImm();
    // Displacement is in units of 2-byte instructions.
    int64_t Disp = Offset / 2;
    // 9-bit signed field
    Disp &= 0x1FF;
    return static_cast<unsigned>(Disp);
  }
  if (MO.isExpr()) {
    // PC-relative: fixup Value will be target_addr - source_addr (bytes).
    // The applyFixup converts it to instruction displacement.
    Fixups.push_back(MCFixup::create(0, MO.getExpr(),
                                     MCFixupKind(ETCA::fixup_ETCA_BASE_JMP),
                                     /*PCRel=*/true));
    return 0;
  }
  return 0;
}

unsigned
ETCAMCCodeEmitter::encodeCallTarget(const MCInst &MI, unsigned OpNo,
                                    SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm()) {
    int64_t Offset = MO.getImm();
    int64_t Disp = Offset / 2;
    Disp &= 0xFFF;
    return static_cast<unsigned>(Disp);
  }
  if (MO.isExpr()) {
    // PC-relative: fixup Value will be target_addr - source_addr (bytes).
    // The applyFixup converts it to instruction displacement.
    Fixups.push_back(MCFixup::create(0, MO.getExpr(),
                                     MCFixupKind(ETCA::fixup_ETCA_SAF_CALL),
                                     /*PCRel=*/true));
    return 0;
  }
  return 0;
}

void ETCAMCCodeEmitter::encodeInstruction(const MCInst &MI,
                                          SmallVectorImpl<char> &CB,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  unsigned Opcode = MI.getOpcode();
  uint16_t Encoding = 0;

  // Helper to extract SS bits from opcode name suffix
  // Returns: 0b00 for 8-bit, 0b01 for 16-bit, 0b10 for 32-bit, 0b11 for 64-bit.
  // Non-sized opcodes default to 0b01 (16-bit).
  auto getSS = [&](unsigned Opc) -> unsigned {
    // Check for 64-bit (SS=11)
    if (Opc == ETCA::ADD64 || Opc == ETCA::SUB64 || Opc == ETCA::RSUB64 ||
        Opc == ETCA::OR64 || Opc == ETCA::XOR64 || Opc == ETCA::AND64 ||
        Opc == ETCA::MOVZ64 || Opc == ETCA::MOVS64 || Opc == ETCA::CMP64 ||
        Opc == ETCA::TEST64 || Opc == ETCA::ADDI64 || Opc == ETCA::SUBI64 ||
        Opc == ETCA::RSUBI64 || Opc == ETCA::CMPI64 || Opc == ETCA::ORI64 ||
        Opc == ETCA::XORI64 || Opc == ETCA::ANDI64 || Opc == ETCA::TESTI64 ||
        Opc == ETCA::MOVZI64 || Opc == ETCA::MOVSI64 || Opc == ETCA::LOAD64 ||
        Opc == ETCA::STORE64)
      return 0b11;
    // Check for 32-bit (SS=10)
    if (Opc == ETCA::ADD32 || Opc == ETCA::SUB32 || Opc == ETCA::RSUB32 ||
        Opc == ETCA::OR32 || Opc == ETCA::XOR32 || Opc == ETCA::AND32 ||
        Opc == ETCA::MOVZ32 || Opc == ETCA::MOVS32 || Opc == ETCA::CMP32 ||
        Opc == ETCA::TEST32 || Opc == ETCA::ADDI32 || Opc == ETCA::SUBI32 ||
        Opc == ETCA::RSUBI32 || Opc == ETCA::CMPI32 || Opc == ETCA::ORI32 ||
        Opc == ETCA::XORI32 || Opc == ETCA::ANDI32 || Opc == ETCA::TESTI32 ||
        Opc == ETCA::MOVZI32 || Opc == ETCA::MOVSI32 || Opc == ETCA::LOAD32 ||
        Opc == ETCA::STORE32)
      return 0b10;
    // Check for 8-bit (SS=00) — BYTE extension
    if (Opc == ETCA::ADD8 || Opc == ETCA::SUB8 || Opc == ETCA::RSUB8 ||
        Opc == ETCA::OR8 || Opc == ETCA::XOR8 || Opc == ETCA::AND8 ||
        Opc == ETCA::MOVZ8 || Opc == ETCA::MOVS8 || Opc == ETCA::CMP8 ||
        Opc == ETCA::TEST8 || Opc == ETCA::ADDI8 || Opc == ETCA::SUBI8 ||
        Opc == ETCA::RSUBI8 || Opc == ETCA::CMPI8 || Opc == ETCA::ORI8 ||
        Opc == ETCA::XORI8 || Opc == ETCA::ANDI8 || Opc == ETCA::TESTI8 ||
        Opc == ETCA::MOVZI8 || Opc == ETCA::MOVSI8 || Opc == ETCA::LOAD8 ||
        Opc == ETCA::STORE8)
      return 0b00;
    return 0b01; // 16-bit (default)
  };

  // RR-format instructions: ADD, SUB, RSUB, OR, XOR, AND
  // Encoding (16-bit LE): (rA << 13) | (rB << 10) | (SS << 4) | CCCC
  //   bits [15:13]=rA, [12:10]=rB, [9:8]=00, [7:6]=00, [5:4]=SS, [3:0]=CCCC
  // Operands: [0]=dst=rA, [1]=src1=rA, [2]=src2=rB  (src1=rA via constraint,
  // same as dst)
  auto encodeRR = [&](unsigned OpcodeVal) {
    unsigned RegA =
        getRegisterOpValue(MI, 1); // src1 (same as dst via tie) => rA
    unsigned RegB = getRegisterOpValue(MI, 2); // src2 => rB
    unsigned SS = getSS(Opcode);
    Encoding = (RegA << 13) | (RegB << 10) | (SS << 4) | OpcodeVal;
  };

  // Non-tied RR format: MOVZ/MOVS (2 operands only: [dst, src])
  // Encoding: (rA << 13) | (rB << 10) | (SS << 4) | CCCC
  // Operands: [0]=dst=rA, [1]=src=rB
  auto encodeRR_NT = [&](unsigned OpcodeVal) {
    unsigned RegDst = getRegisterOpValue(MI, 0); // dst => rA
    unsigned RegSrc = getRegisterOpValue(MI, 1); // src => rB
    unsigned SS = getSS(Opcode);
    Encoding = (RegDst << 13) | (RegSrc << 10) | (SS << 4) | OpcodeVal;
  };

  // RI-format instructions: ADDI, SUBI, RSUBI, CMPI, ORI, XORI, ANDI,
  // TESTI, SLO, READCR, WRITECR
  // Encoding (16-bit LE): (rA << 13) | (imm << 8) | (0x01 << 6) | (SS << 4) |
  // CCCC
  //   bits [15:13]=rA, [12:8]=imm, [7:6]=01(fmt), [5:4]=SS, [3:0]=CCCC
  // Two operand layouts:
  //   Standard (dst, src1, imm): dst=op0, src1=op1, imm=op2
  //   CmpLike (src1, imm): src1=op0, imm=op1  (no output register)
  auto encodeRI = [&](unsigned OpcodeVal, bool IsCmpLike = false) {
    unsigned RegA = getRegisterOpValue(MI, IsCmpLike ? 0 : 1);
    unsigned Imm = getImmOpValue(MI, IsCmpLike ? 1 : 2, Fixups,
                                 MCFixupKind(ETCA::fixup_ETCA_NONE));
    unsigned SS = getSS(Opcode);
    Encoding = (RegA << 13) | ((Imm & 0x1F) << 8) | (0x01 << 6) | (SS << 4) |
               OpcodeVal;
  };

  // Non-tied RI format: MOVZI/MOVSI (2 operands only: [dst, imm])
  // Encoding: (rA << 13) | (imm << 8) | (0x01 << 6) | (SS << 4) | CCCC
  // Operands: [0]=dst=rA, [1]=imm
  auto encodeRI_NT = [&](unsigned OpcodeVal) {
    unsigned RegDst = getRegisterOpValue(MI, 0); // dst => rA
    unsigned Imm =
        getImmOpValue(MI, 1, Fixups, MCFixupKind(ETCA::fixup_ETCA_NONE));
    unsigned SS = getSS(Opcode);
    Encoding = (RegDst << 13) | ((Imm & 0x1F) << 8) | (0x01 << 6) | (SS << 4) |
               OpcodeVal;
  };

  switch (Opcode) {
  // === RR instructions (opcode: 0-9) — all width variants ===
  case ETCA::ADD16:
  case ETCA::ADD32:
  case ETCA::ADD64:
  case ETCA::ADD8:
    encodeRR(0);
    break;
  case ETCA::SUB16:
  case ETCA::SUB32:
  case ETCA::SUB64:
  case ETCA::SUB8:
    encodeRR(1);
    break;
  case ETCA::RSUB16:
  case ETCA::RSUB32:
  case ETCA::RSUB64:
  case ETCA::RSUB8:
    encodeRR(2);
    break;
  case ETCA::OR16:
  case ETCA::OR32:
  case ETCA::OR64:
  case ETCA::OR8:
    encodeRR(4);
    break;
  case ETCA::XOR16:
  case ETCA::XOR32:
  case ETCA::XOR64:
  case ETCA::XOR8:
    encodeRR(5);
    break;
  case ETCA::AND16:
  case ETCA::AND32:
  case ETCA::AND64:
  case ETCA::AND8:
    encodeRR(6);
    break;
  case ETCA::MOVZ16:
  case ETCA::MOVZ32:
  case ETCA::MOVZ64:
  case ETCA::MOVZ8:
    encodeRR_NT(8);
    break;
  case ETCA::MOVS16:
  case ETCA::MOVS32:
  case ETCA::MOVS64:
  case ETCA::MOVS8:
    encodeRR_NT(9);
    break;

  // === RI instructions (opcode: 0-9, 12, 14, 15) — all width variants ===
  case ETCA::ADDI16:
  case ETCA::ADDI32:
  case ETCA::ADDI64:
  case ETCA::ADDI8:
    encodeRI(0);
    break;
  case ETCA::SUBI16:
  case ETCA::SUBI32:
  case ETCA::SUBI64:
  case ETCA::SUBI8:
    encodeRI(1);
    break;
  case ETCA::RSUBI16:
  case ETCA::RSUBI32:
  case ETCA::RSUBI64:
  case ETCA::RSUBI8:
    encodeRI(2);
    break;
  case ETCA::CMPI16:
  case ETCA::CMPI32:
  case ETCA::CMPI64:
  case ETCA::CMPI8:
    encodeRI(3);
    break;
  case ETCA::ORI16:
  case ETCA::ORI32:
  case ETCA::ORI64:
  case ETCA::ORI8:
    encodeRI(4);
    break;
  case ETCA::XORI16:
  case ETCA::XORI32:
  case ETCA::XORI64:
  case ETCA::XORI8:
    encodeRI(5);
    break;
  case ETCA::ANDI16:
  case ETCA::ANDI32:
  case ETCA::ANDI64:
  case ETCA::ANDI8:
    encodeRI(6);
    break;
  case ETCA::TESTI16:
  case ETCA::TESTI32:
  case ETCA::TESTI64:
  case ETCA::TESTI8:
    encodeRI(7);
    break;
  case ETCA::MOVZI16:
  case ETCA::MOVZI32:
  case ETCA::MOVZI64:
  case ETCA::MOVZI8:
    encodeRI_NT(8);
    break;
  case ETCA::MOVSI16:
  case ETCA::MOVSI32:
  case ETCA::MOVSI64:
  case ETCA::MOVSI8:
    encodeRI_NT(9);
    break;
  case ETCA::SLO16:
    encodeRI(12);
    break;
  case ETCA::READCR:
    encodeRI(14);
    break;
  case ETCA::WRITECR:
    encodeRI(15);
    break;

  // === RR-format LOAD/STORE: all widths (8/16/32/64) ===
  // Encoding: (rA << 13) | (rB << 10) | (SS << 4) | CCCC
  //   LOAD: CCCC = 1010, STORE: CCCC = 1011
  // Operands: [0]=dst/val=rA, [1]=addr=rB
  case ETCA::LOAD8:
  case ETCA::LOAD16:
  case ETCA::LOAD32:
  case ETCA::LOAD64:
  case ETCA::STORE8:
  case ETCA::STORE16:
  case ETCA::STORE32:
  case ETCA::STORE64: {
    bool IsLoad = (Opcode == ETCA::LOAD8 || Opcode == ETCA::LOAD16 ||
                   Opcode == ETCA::LOAD32 || Opcode == ETCA::LOAD64);
    unsigned RegA = getRegisterOpValue(MI, 0); // dst/val => rA
    unsigned RegB = getRegisterOpValue(MI, 1); // addr => rB
    unsigned SS = getSS(Opcode);
    Encoding =
        (RegA << 13) | (RegB << 10) | (SS << 4) | (IsLoad ? 0b1010 : 0b1011);
    break;
  }

  // === CMP/TEST (RR format): (rA << 13) | (rB << 10) | (SS << 4) | CCCC
  case ETCA::CMP8:
  case ETCA::CMP:
  case ETCA::CMP32:
  case ETCA::CMP64: {
    unsigned Src1 = getRegisterOpValue(MI, 0); // rA
    unsigned Src2 = getRegisterOpValue(MI, 1); // rB
    // CMP: CCCC=0011, SS from opcode
    unsigned SS = getSS(Opcode);
    Encoding = (Src1 << 13) | (Src2 << 10) | (SS << 4) | 0b0011;
    break;
  }
  case ETCA::TEST8:
  case ETCA::TEST:
  case ETCA::TEST32:
  case ETCA::TEST64: {
    unsigned Src1 = getRegisterOpValue(MI, 0); // rA
    unsigned Src2 = getRegisterOpValue(MI, 1); // rB
    // TEST: CCCC=0111, SS from opcode
    unsigned SS = getSS(Opcode);
    Encoding = (Src1 << 13) | (Src2 << 10) | (SS << 4) | 0b0111;
    break;
  }

  // === Branch instructions ===
  // Byte 0: bit[7]=1 | bits[6:5]=00 | bit[4]=D8 | bits[3:0]=CCCC
  // Byte 1: D[7:0]
  // Encoding = (0x80 | Cond | ((Disp >> 8) & 1) << 4) | ((Disp & 0xFF) << 8)
  // Condition codes match binutils (etca-binutils-gdb).
  case ETCA::BR:
  case ETCA::BEQ:
  case ETCA::BNE:
  case ETCA::BLT:
  case ETCA::BGE:
  case ETCA::BLTU:
  case ETCA::BGEU:
  case ETCA::BLE:
  case ETCA::BGT:
  case ETCA::BLEU:
  case ETCA::BGTU: {
    unsigned Cond;
    switch (Opcode) {
    case ETCA::BR:
      Cond = 14;
      break;
    case ETCA::BEQ:
      Cond = 0;
      break;
    case ETCA::BNE:
      Cond = 1;
      break;
    case ETCA::BLT:
      Cond = 10;
      break; // binutils: jl/jnge
    case ETCA::BGE:
      Cond = 11;
      break; // binutils: jge/jnl
    case ETCA::BLTU:
      Cond = 4;
      break; // binutils: jb/jc/jnae
    case ETCA::BGEU:
      Cond = 5;
      break; // binutils: jae/jnb/jnc
    case ETCA::BLE:
      Cond = 12;
      break; // binutils: jle/jng
    case ETCA::BGT:
      Cond = 13;
      break; // binutils: jg/jnle
    case ETCA::BLEU:
      Cond = 8;
      break; // binutils: jbe/jna
    case ETCA::BGTU:
      Cond = 9;
      break; // binutils: ja/jnbe
    default:
      llvm_unreachable("bad branch opcode");
    }
    unsigned Disp = encodeBranchTarget(MI, 0, Fixups);
    unsigned Byte0 = 0x80 | Cond | (((Disp >> 8) & 1) << 4);
    unsigned Byte1 = Disp & 0xFF;
    Encoding = Byte0 | (Byte1 << 8);
    break;
  }

  // === NOP: 0x008F (2-byte NOP per binutils etca_build_nop) ===
  case ETCA::NOP:
    Encoding = 0x008F;
    break;

  // === Pseudo-instructions (lowered before emission, but emit NOP if seen) ===
  case ETCA::RET_Pseudo:
  case ETCA::CALL_Pseudo:
  case ETCA::SELECT_Pseudo:
  case ETCA::ICMP_Pseudo:
    Encoding = 0x008F;
    break;

  // === SAF: PUSH register (RR format) ===
  // Encoding: (sp << 13) | (Reg << 10) | (SS << 4) | 0xD
  //   rA=sp=6, rB=Reg, CCCC=1101
  case ETCA::PUSH: {
    unsigned Reg = getRegisterOpValue(MI, 0); // register to push => rB
    unsigned SS = getSS(Opcode);
    Encoding = (6 << 13) | (Reg << 10) | (SS << 4) | 0b1101;
    break;
  }

  // === SAF: POP register (RR format) ===
  // Encoding: (Reg << 13) | (sp << 10) | (SS << 4) | 0xC
  //   rA=Reg, rB=sp=6, CCCC=1100
  case ETCA::POP: {
    unsigned Reg = getRegisterOpValue(MI, 0); // register to pop => rA
    unsigned SS = getSS(Opcode);
    Encoding = (Reg << 13) | (6 << 10) | (SS << 4) | 0b1100;
    break;
  }

  // === SAF: PUSH immediate (RI format) ===
  // Encoding: (110 << 13) | (imm << 8) | (0x01 << 6) | (SS << 4) | 0xD
  //   rA=110(fixed), CCCC=1101
  case ETCA::PUSHI: {
    unsigned Imm =
        getImmOpValue(MI, 0, Fixups, MCFixupKind(ETCA::fixup_ETCA_NONE));
    unsigned SS = getSS(Opcode);
    Encoding =
        (6 << 13) | ((Imm & 0x1F) << 8) | (0x01 << 6) | (SS << 4) | 0b1101;
    break;
  }

  // === SAF: CALL (byte0=1011 D[11:8], byte1=D[7:0]) 12-bit PC-relative ===
  case ETCA::CALL: {
    unsigned Disp = encodeCallTarget(MI, 0, Fixups);
    // Byte 0 = 1011 (format) | D[11:8], Byte 1 = D[7:0]
    // Written LE: CB[0]=byte0, CB[1]=byte1
    Encoding = (0xB0 | ((Disp >> 8) & 0xF)) // byte 0: format + disp high nibble
               | ((Disp & 0xFF) << 8);      // byte 1: disp low byte
    break;
  }

  // === SAF: JMPR (byte0=10101111=0xAF, byte1=RRR[7:5]|X[4]|CCCC[3:0]) ===
  case ETCA::JMPR: {
    unsigned Reg = getRegisterOpValue(MI, 0);
    // Byte 0 = 0xAF, Byte 1 = RRR[7:5] | X=0[4] | cond=1110[3:0]
    Encoding = 0x00AF | (Reg << 13) | (0xE << 8);
    break;
  }

  // === SAF: CALLR (byte0=10101111=0xAF, byte1=RRR[7:5]|X[4]|CCCC[3:0]) ===
  case ETCA::CALLR: {
    unsigned Reg = getRegisterOpValue(MI, 0);
    // Byte 0 = 0xAF, Byte 1 = RRR[7:5] | X=1[4] | cond=1110[3:0]
    Encoding = 0x00AF | (Reg << 13) | (1 << 12) | (0xE << 8);
    break;
  }

  default:
    llvm_unreachable("Unknown ETCA opcode in encoder");
    break;
  }

  // ETCA is little-endian: write low byte first, then high byte.
  CB.push_back(static_cast<char>(Encoding & 0xFF));
  CB.push_back(static_cast<char>((Encoding >> 8) & 0xFF));
}

MCCodeEmitter *llvm::createETCAMCCodeEmitter(const MCInstrInfo &MCII,
                                             MCContext &Ctx) {
  return new ETCAMCCodeEmitter(MCII, Ctx);
}

//===----------------------------------------------------------------------===//
// ETCAAsmBackend
//===----------------------------------------------------------------------===//

namespace {
class ETCAAsmBackend : public MCAsmBackend {
public:
  ETCAAsmBackend(const MCSubtargetInfo &STI, const MCTargetOptions &Options)
      : MCAsmBackend(llvm::endianness::little) {}

  ~ETCAAsmBackend() override = default;

  void applyFixup(const MCFragment &Frag, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override {
    MCFixupKind Kind = Fixup.getKind();
    if (Kind == ETCA::fixup_ETCA_NONE)
      return;

    // If not resolved, record a relocation and leave the encoding as-is
    // (the encoder already wrote 0 for the fixup field).
    if (!IsResolved) {
      maybeAddReloc(Frag, Fixup, Target, Value, IsResolved);
      return;
    }

    if (!Data)
      return;

    // Data already points to fragment_data + Fixup.getOffset(),
    // so we write directly to the data buffer (no extra Offset addition).
    switch (Kind) {
    case ETCA::fixup_ETCA_BASE_JMP: {
      // 9-bit PC-relative branch displacement.
      // Instruction encoding (LE 16-bit):
      //   byte0 bit 4 = D8 (disp bit 8)
      //   byte1 = D[7:0]
      //   16-bit: D8 at bit 4, D[7:0] at bits [15:8]
      // Value is the resolved difference in bytes between target and source.
      // LLVM's MC fixup system computes this as (target - (source + Size)).
      // Since ETCa branches are relative in halfwords, divide by 2.
      int64_t Disp = static_cast<int64_t>(Value) / 2;
      if (!isInt<9>(Disp))
        report_fatal_error("ETCA branch target out of range");

      uint16_t &Insn = *reinterpret_cast<uint16_t *>(Data);
      // Preserve condition code (bits 7,6,5,3,2,1,0 of byte 0)
      Insn &= 0x00EF; // Clear D8 (bit 4) and D[7:0] (bits [15:8])
      Insn |= (((Disp >> 8) & 1) << 4) // D8 at bit 4
              | ((Disp & 0xFF) << 8);  // D[7:0] at bits [15:8]
      break;
    }
    case ETCA::fixup_ETCA_SAF_CALL: {
      // 12-bit PC-relative call displacement.
      // Instruction format: byte0=0xB0|D[11:8], byte1=D[7:0]
      // Value is the resolved PC-relative difference in bytes.
      int64_t Disp = static_cast<int64_t>(Value) / 2;
      if (!isInt<12>(Disp))
        report_fatal_error("ETCA call target out of range");

      uint16_t &Insn = *reinterpret_cast<uint16_t *>(Data);
      // Clear existing displacement, keep format nibble (byte0 bits [7:4])
      Insn &= 0x00F0;                      // Keep byte0 bits [7:4], clear rest
      Insn |= (0xB0)                       // byte0 high nibble = 0xB
              | (((Disp >> 8) & 0xF) << 0) // D[11:8] in byte0 low nibble
              | ((Disp & 0xFF) << 8);      // D[7:0] in byte1
      break;
    }
    case ETCA::fixup_ETCA_8: {
      uint8_t &Byte = *reinterpret_cast<uint8_t *>(Data);
      Byte = uint8_t(Value & 0xFF);
      break;
    }
    case ETCA::fixup_ETCA_16: {
      uint16_t &Half = *reinterpret_cast<uint16_t *>(Data);
      Half = uint16_t(Value & 0xFFFF);
      break;
    }
    case ETCA::fixup_ETCA_32: {
      uint32_t &Word = *reinterpret_cast<uint32_t *>(Data);
      Word = uint32_t(Value & 0xFFFFFFFF);
      break;
    }
    case ETCA::fixup_ETCA_64: {
      uint64_t &Long = *reinterpret_cast<uint64_t *>(Data);
      Long = Value;
      break;
    }
    default:
      llvm_unreachable("unknown ETCA fixup kind");
    }
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createETCAELFObjectWriter(0);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    static const MCFixupKindInfo Infos[] = {
        {"fixup_ETCA_NONE", 0, 0, 0},      {"fixup_ETCA_BASE_JMP", 0, 16, 0},
        {"fixup_ETCA_8", 0, 8, 0},         {"fixup_ETCA_16", 0, 16, 0},
        {"fixup_ETCA_32", 0, 32, 0},       {"fixup_ETCA_64", 0, 64, 0},
        {"fixup_ETCA_SAF_CALL", 0, 16, 0},
    };
    enum { NumETCAFixups = std::size(Infos) };

    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);
    unsigned Index = Kind - FirstTargetFixupKind;
    if (Index < NumETCAFixups)
      return Infos[Index];
    return {"fixup_ETCA_unknown", 0, 16, 0};
  }

  unsigned getMinimumNopSize() const override { return 2; }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    // NOP = add r0, r0 (0x0010)
    if ((Count % 2) != 0)
      return false;
    for (uint64_t i = 0; i < Count; i += 2)
      OS.write("\x10\x00", 2);
    return true;
  }
};
} // namespace

MCAsmBackend *llvm::createETCAAsmBackend(const Target &T,
                                         const MCSubtargetInfo &STI,
                                         const MCRegisterInfo &MRI,
                                         const MCTargetOptions &Options) {
  return new ETCAAsmBackend(STI, Options);
}

//===----------------------------------------------------------------------===//
// ELF Object Writer
//===----------------------------------------------------------------------===//

namespace {

// ETCA ELF relocation types — must match binutils include/elf/etca.h
enum ETCARelocType : unsigned {
  R_ETCA_NONE = 0,
  R_ETCA_BASE_JMP = 1,
  R_ETCA_EXABS_8 = 2,
  R_ETCA_EXABS_16 = 3,
  R_ETCA_EXABS_32 = 4,
  R_ETCA_EXABS_64 = 5,
  R_ETCA_SAF_CALL = 6,
  R_ETCA_8 = 49,
  R_ETCA_16 = 50,
  R_ETCA_32 = 51,
  R_ETCA_64 = 52,
  R_ETCA_IPREL_8 = 53,
  R_ETCA_IPREL_16 = 54,
  R_ETCA_IPREL_32 = 55,
  R_ETCA_IPREL_64 = 56,
};

class ETCAELFObjectWriter : public MCELFObjectTargetWriter {
public:
  ETCAELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit=*/false, OSABI, ELF::EM_ETCA,
                                /*HasRelocationAddend=*/false) {}

  ~ETCAELFObjectWriter() override = default;

  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    MCFixupKind Kind = Fixup.getKind();
    if (Kind < FirstTargetFixupKind)
      return R_ETCA_NONE;

    unsigned ETCAKind = Kind - FirstTargetFixupKind;

    switch (ETCAKind) {
    case ETCA::fixup_ETCA_NONE:
      return R_ETCA_NONE;
    case ETCA::fixup_ETCA_BASE_JMP:
      return R_ETCA_BASE_JMP;
    case ETCA::fixup_ETCA_8:
      return R_ETCA_8;
    case ETCA::fixup_ETCA_16:
      return R_ETCA_16;
    case ETCA::fixup_ETCA_32:
      return R_ETCA_32;
    case ETCA::fixup_ETCA_64:
      return R_ETCA_64;
    case ETCA::fixup_ETCA_SAF_CALL:
      return R_ETCA_SAF_CALL;
    default:
      return R_ETCA_NONE;
    }
  }
};

} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createETCAELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<ETCAELFObjectWriter>(OSABI);
}

//===----------------------------------------------------------------------===//
// LLVM initialization
//===----------------------------------------------------------------------===//

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeETCATargetMC() {
  TargetRegistry::RegisterMCRegInfo(getTheETCATarget(),
                                    createETCAMCRegisterInfo);
  TargetRegistry::RegisterMCInstrInfo(getTheETCATarget(),
                                      createETCAMCInstrInfo);
  TargetRegistry::RegisterMCSubtargetInfo(getTheETCATarget(),
                                          createETCAMCSubtargetInfo);
  TargetRegistry::RegisterMCAsmInfo(getTheETCATarget(), createETCAMCAsmInfo);
  TargetRegistry::RegisterMCInstPrinter(getTheETCATarget(),
                                        createETCAMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(getTheETCATarget(),
                                        createETCAMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(getTheETCATarget(),
                                       createETCAAsmBackend);
}
