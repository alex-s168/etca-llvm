//===-- ETCAAsmParser.cpp - Parse ETCA assembly to MCInst ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ETCA Assembly Parser.
//
// Parses ETCA assembly syntax into MCInst structures.  ETCA is a simple
// 16-bit fixed-width RISC ISA with 8 registers (r0-r7) in the base ISA,
// plus d0-d7 (32-bit DW extension) and q0-q7 (64-bit QW extension).
//
// All instructions are 2 bytes, little-endian.
//
// Assembly syntax (matches binutils):
//   RR instructions:       add %r0, %r1        (dst=src1, src2)
//   RI instructions:       add %r0, 5          (dst=src1, imm5)
//   LOAD/STORE:            load %r0, %r1 / store %r0, %r1
//   CMP/TEST:              cmp %r0, %r1 / test %r0, %r1
//   Branch instructions:   br target / beq target / ...
//   SAF instructions:      call target / push %r0 / pop %r0
//                          jmpr %r0 / callr %r0
//   NOP:                   nop
//
// Register naming (binutils-compatible):
//   %rN  or rN    = 16-bit (word) register, e.g., %r0, %r1
//   %rNd or rNd   = 32-bit (dword) register, e.g., %r0d, %r1d
//   %rNq or rNq   = 64-bit (qword) register, e.g., %r0q, %r1q
//   %rNx or rNx   = 16-bit (explicit word), e.g., %r0x (alias for rN)
//   %rNh or rNh   = 8-bit (byte), e.g., %r0h (reserved for BYTE extension)
//   %dN  or dN    = 32-bit register (backward compat), e.g., %d0
//   %qN  or qN    = 64-bit register (backward compat), e.g., %q0
//   ABI names:     %a0=%r0, %a1=%r1, %a2=%r2, %s0=%r3,
//                  %s1=%r4, %bp=%r5, %sp=%r6, %ln=%r7
//
// The '%' prefix is optional but recommended for binutils compatibility.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ETCAFixupKinds.h"
#include "MCTargetDesc/ETCAMCTargetDesc.h"
#include "TargetInfo/ETCATargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"

#include <cstring>

using namespace llvm;

#define DEBUG_TYPE "etca-asm-parser"

// GET_REGINFO_ENUM and GET_SUBTARGETINFO_ENUM are provided by
// ETCAMCTargetDesc.h
#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"

namespace {

//===------------------------------------------------------------------===//
// ETCAOperand — A parsed ETCA assembly operand.
//===------------------------------------------------------------------===//

class ETCAOperand : public MCParsedAsmOperand {
public:
  enum KindTy {
    k_Register,   // rN, dN, qN
    k_Immediate,  // integer literal (absolute value, known at parse time)
    k_Expression, // label / symbol expression (branch targets, fixups)
    k_Token,      // instruction mnemonic placeholder
    k_Memory,     // [reg] — memory operand (lowered to LOAD/STORE)
  } Kind;

  SMLoc StartLoc, EndLoc;

private:
  struct RegOperand {
    unsigned RegNum;
    unsigned WidthHint; // 8, 16, 32, or 64 — derived from register name suffix
  };
  struct ImmOperand {
    int64_t Value;
  };
  struct MemOperand {
    unsigned RegNum;
    unsigned WidthHint;
  };
  union {
    RegOperand Reg;
    ImmOperand Imm;
    MemOperand Mem;
  };
  const MCExpr *Expr = nullptr;

public:
  ETCAOperand(KindTy K, SMLoc S, SMLoc E) : Kind(K), StartLoc(S), EndLoc(E) {
    if (K == k_Register)
      Reg.WidthHint = 16; // default
  }

  // --- Factory methods ---
  static std::unique_ptr<ETCAOperand>
  createReg(unsigned RegNum, unsigned WidthHint, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<ETCAOperand>(k_Register, S, E);
    Op->Reg.RegNum = RegNum;
    Op->Reg.WidthHint = WidthHint;
    return Op;
  }

  static std::unique_ptr<ETCAOperand> createImm(int64_t Val, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<ETCAOperand>(k_Immediate, S, E);
    Op->Imm.Value = Val;
    return Op;
  }

  static std::unique_ptr<ETCAOperand> createExpr(const MCExpr *ExprVal, SMLoc S,
                                                 SMLoc End) {
    auto Op = std::make_unique<ETCAOperand>(k_Expression, S, End);
    Op->Expr = ExprVal;
    return Op;
  }

  static std::unique_ptr<ETCAOperand>
  createMem(unsigned RegNum, unsigned WidthHint, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<ETCAOperand>(k_Memory, S, E);
    Op->Mem.RegNum = RegNum;
    Op->Mem.WidthHint = WidthHint;
    return Op;
  }

  static std::unique_ptr<ETCAOperand> createTok(SMLoc S, SMLoc E) {
    return std::make_unique<ETCAOperand>(k_Token, S, E);
  }

  // --- MCParsedAsmOperand overrides ---
  bool isReg() const override { return Kind == k_Register; }
  bool isImm() const override { return Kind == k_Immediate; }
  bool isToken() const override { return Kind == k_Token; }
  bool isMem() const override { return Kind == k_Memory; }
  bool isMemReg() const { return Kind == k_Memory; }

  bool isExpr() const { return Kind == k_Expression; }

  MCRegister getReg() const override {
    assert(isReg());
    return Reg.RegNum;
  }

  int64_t getImm() const {
    assert(isImm());
    return Imm.Value;
  }

  const MCExpr *getExpr() const {
    assert(isExpr());
    return Expr;
  }

  unsigned getRegisterWidth() const {
    assert(isReg());
    // Use the explicit width hint from the register name suffix if available.
    if (Reg.WidthHint)
      return Reg.WidthHint;
    // Fallback: infer from register class.
    unsigned R = Reg.RegNum;
    if (R >= ETCA::Q0 && R <= ETCA::Q7)
      return 64;
    if (R >= ETCA::D0 && R <= ETCA::D7)
      return 32;
    return 16;
  }

  unsigned getMemReg() const {
    assert(isMem());
    return Mem.RegNum;
  }
  unsigned getMemWidthHint() const {
    assert(isMem());
    return Mem.WidthHint;
  }

  unsigned getRegIdx() const {
    assert(isReg());
    unsigned R = Reg.RegNum;
    if (R >= ETCA::R0 && R <= ETCA::R7)
      return R - ETCA::R0;
    if (R >= ETCA::D0 && R <= ETCA::D7)
      return R - ETCA::D0;
    if (R >= ETCA::Q0 && R <= ETCA::Q7)
      return R - ETCA::Q0;
    return 0;
  }

  // --- Standard operand adders (used by auto-generated code path) ---
  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1);
    Inst.addOperand(MCOperand::createReg(getReg()));
  }
  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1);
    Inst.addOperand(MCOperand::createImm(getImm()));
  }
  void addExprOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1);
    Inst.addOperand(MCOperand::createExpr(getExpr()));
  }

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  void print(raw_ostream &OS, const MCAsmInfo &) const override {
    switch (Kind) {
    case k_Register:
      OS << "reg(" << (getReg() % 8) << ")";
      break;
    case k_Immediate:
      OS << "imm(" << getImm() << ")";
      break;
    case k_Expression:
      OS << "expr";
      break;
    case k_Token:
      OS << "token";
      break;
    case k_Memory:
      OS << "mem[" << (getMemReg() % 8) << "]";
      break;
    }
  }
};

//===------------------------------------------------------------------===//
// ETCAAsmParser — Main assembly parser.
//===------------------------------------------------------------------===//

class ETCAAsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;
  const MCRegisterInfo &MRI;

  /// Mnemonic of the instruction currently being parsed.
  std::string CurrentMnemonic;
  /// Width hint from instruction size suffix (0 if none).
  unsigned CurrentInstrWidth = 0;

  // Opcode lookup tables.
  struct RrRiEntry {
    const char *Mnemonic;
    unsigned Opcode8RR;
    unsigned Opcode16RR;
    unsigned Opcode32RR;
    unsigned Opcode64RR;
    unsigned Opcode8RI;
    unsigned Opcode16RI;
    unsigned Opcode32RI;
    unsigned Opcode64RI;
  };
  static const RrRiEntry ALUOps[];

  struct BranchEntry {
    const char *Mnemonic;
    unsigned Opcode;
  };
  static const BranchEntry BranchOps[];

  struct FixedEntry {
    const char *Mnemonic;
    unsigned Opcode;
    unsigned NumOps;   // expected operand count
    bool IsTargetExpr; // true if operand is a branch/call target expression
    bool IsMov;        // true if this is the mov pseudo-instruction
  };
  static const FixedEntry FixedOps[];

public:
  ETCAAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII), Parser(Parser),
        MRI(*Parser.getContext().getRegisterInfo()) {
    MCAsmParserExtension::Initialize(Parser);
    setAvailableFeatures(FeatureBitset());
  }

  // --- Required MCTargetAsmParser overrides ---
  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;
  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;
  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override {
    return Match_InvalidOperand;
  }

  void convertToMapAndConstraints(unsigned Kind,
                                  const OperandVector &Operands) override {
    // No conversion needed for manual matching.
  }

private:
  /// Parse a single operand: register, immediate, or expression.
  bool parseOperand(OperandVector &Operands);

  /// Match a register name string (rN, dN, qN) to a MCRegister.
  MCRegister matchRegisterName(StringRef Name);

  /// Extract the width hint from a register name suffix.
  /// Returns 8 for 'h', 16 for 'x' or no suffix, 32 for 'd', 64 for 'q'.
  static unsigned getWidthHintFromName(StringRef Name);

  /// Look up a branch opcode by mnemonic.
  static unsigned lookupBranch(StringRef Mnemonic);

  /// Look up a fixed-form opcode by mnemonic.
  static const FixedEntry *lookupFixed(StringRef Mnemonic);

  /// Look up an ALU opcode entry by mnemonic.
  static const RrRiEntry *lookupALU(StringRef Mnemonic);

  /// Select width-specific opcode for an ALU operation.
  static unsigned selectWidth(const RrRiEntry &E, bool IsRR, unsigned Width);
};

} // end anonymous namespace

//===------------------------------------------------------------------===//
// Opcode lookup tables
//===------------------------------------------------------------------===//

// ALU operations: mnemonic → {8RR, 16RR, 32RR, 64RR, 8RI, 16RI, 32RI, 64RI}
// Entries with 0 mean "not available in this width/form".
const ETCAAsmParser::RrRiEntry ETCAAsmParser::ALUOps[] = {
    {"add", ETCA::ADD8, ETCA::ADD16, ETCA::ADD32, ETCA::ADD64, ETCA::ADDI8,
     ETCA::ADDI16, ETCA::ADDI32, ETCA::ADDI64},
    {"sub", ETCA::SUB8, ETCA::SUB16, ETCA::SUB32, ETCA::SUB64, ETCA::SUBI8,
     ETCA::SUBI16, ETCA::SUBI32, ETCA::SUBI64},
    {"rsub", ETCA::RSUB8, ETCA::RSUB16, ETCA::RSUB32, ETCA::RSUB64,
     ETCA::RSUBI8, ETCA::RSUBI16, ETCA::RSUBI32, ETCA::RSUBI64},
    {"or", ETCA::OR8, ETCA::OR16, ETCA::OR32, ETCA::OR64, ETCA::ORI8,
     ETCA::ORI16, ETCA::ORI32, ETCA::ORI64},
    {"xor", ETCA::XOR8, ETCA::XOR16, ETCA::XOR32, ETCA::XOR64, ETCA::XORI8,
     ETCA::XORI16, ETCA::XORI32, ETCA::XORI64},
    {"and", ETCA::AND8, ETCA::AND16, ETCA::AND32, ETCA::AND64, ETCA::ANDI8,
     ETCA::ANDI16, ETCA::ANDI32, ETCA::ANDI64},
    {"movz", ETCA::MOVZ8, ETCA::MOVZ16, ETCA::MOVZ32, ETCA::MOVZ64,
     ETCA::MOVZI8, ETCA::MOVZI16, ETCA::MOVZI32, ETCA::MOVZI64},
    {"movs", ETCA::MOVS8, ETCA::MOVS16, ETCA::MOVS32, ETCA::MOVS64,
     ETCA::MOVSI8, ETCA::MOVSI16, ETCA::MOVSI32, ETCA::MOVSI64},
    // CMP/TEST in RR form: width-selectable via SS bits
    {"cmp", ETCA::CMP8, ETCA::CMP, ETCA::CMP32, ETCA::CMP64, ETCA::CMPI8,
     ETCA::CMPI16, ETCA::CMPI32, ETCA::CMPI64},
    {"test", ETCA::TEST8, ETCA::TEST, ETCA::TEST32, ETCA::TEST64, ETCA::TESTI8,
     ETCA::TESTI16, ETCA::TESTI32, ETCA::TESTI64},
    {"slo", 0, 0, 0, 0, 0, ETCA::SLO16, 0, 0}, // RI only, 16-bit only
    {"readcr", 0, 0, 0, 0, 0, ETCA::READCR, 0, 0},
    {"writecr", 0, 0, 0, 0, 0, ETCA::WRITECR, 0, 0},
};

// Branch instructions — both LLVM-style and binutils-compatible names.
const ETCAAsmParser::BranchEntry ETCAAsmParser::BranchOps[] = {
    // LLVM-style and binutils-compatible names
    {"br", ETCA::BR},     {"jmp", ETCA::BR},    {"j", ETCA::BR},
    {"beq", ETCA::BEQ},   {"je", ETCA::BEQ},    {"jz", ETCA::BEQ},
    {"bne", ETCA::BNE},   {"jne", ETCA::BNE},   {"jnz", ETCA::BNE},
    {"blt", ETCA::BLT},   {"jl", ETCA::BLT},    {"jn", ETCA::BLT},
    {"bge", ETCA::BGE},   {"jge", ETCA::BGE},   {"jnn", ETCA::BGE},
    {"bltu", ETCA::BLTU}, {"jlo", ETCA::BLTU},  {"jb", ETCA::BLTU},
    {"bgeu", ETCA::BGEU}, {"jhs", ETCA::BGEU},  {"jae", ETCA::BGEU},
    {"jnb", ETCA::BGEU},  {"ble", ETCA::BLE},   {"jle", ETCA::BLE},
    {"jng", ETCA::BLE},   {"bgt", ETCA::BGT},   {"jg", ETCA::BGT},
    {"jnle", ETCA::BGT},  {"bleu", ETCA::BLEU}, {"jls", ETCA::BLEU},
    {"jna", ETCA::BLEU},  {"jbe", ETCA::BLEU},  {"bgtu", ETCA::BGTU},
    {"ja", ETCA::BGTU},
};

// Fixed-format instructions: LOAD, STORE, SAF ops, NOP.
// NumOps is the number of operand operands (not counting the mnemonic token).
// IsTargetExpr: true for branch/call targets (parsed as expressions).
const ETCAAsmParser::FixedEntry ETCAAsmParser::FixedOps[] = {
    // LOAD/STORE use RR format.  The AsmParser selects width-specific opcode
    // (LOAD8/LOAD16/LOAD32/LOAD64) based on the register width hint.
    // We only need the FixedEntry for the mnemonic lookup; the actual opcode
    // is determined dynamically in the Load/Store handler below.
    {"load", ETCA::LOAD16, 2, false, false},
    {"store", ETCA::STORE16, 2, false, false},
    {"call", ETCA::CALL, 1, true, false},
    {"push", ETCA::PUSH, 1, false, false},
    {"pop", ETCA::POP, 1, false, false},
    {"jmpr", ETCA::JMPR, 1, false, false},
    {"callr", ETCA::CALLR, 1, false, false},
    {"nop", ETCA::NOP, 0, false, false},
    {"mov", ETCA::MOVZ16, 0, false,
     true}, // mov pseudo: width determined by suffix/register
};

//===------------------------------------------------------------------===//
// Lookup helpers
//===------------------------------------------------------------------===//

unsigned ETCAAsmParser::lookupBranch(StringRef Mnemonic) {
  for (const auto &E : BranchOps) {
    if (Mnemonic == E.Mnemonic)
      return E.Opcode;
  }
  return 0;
}

const ETCAAsmParser::FixedEntry *
ETCAAsmParser::lookupFixed(StringRef Mnemonic) {
  for (const auto &E : FixedOps) {
    if (Mnemonic == E.Mnemonic)
      return &E;
  }
  return nullptr;
}

const ETCAAsmParser::RrRiEntry *ETCAAsmParser::lookupALU(StringRef Mnemonic) {
  // Case-insensitive compare
  for (const auto &E : ALUOps) {
    if (Mnemonic.equals_insensitive(E.Mnemonic))
      return &E;
  }
  return nullptr;
}

unsigned ETCAAsmParser::selectWidth(const RrRiEntry &E, bool IsRR,
                                    unsigned Width) {
  switch (Width) {
  case 64:
    return IsRR ? E.Opcode64RR : E.Opcode64RI;
  case 32:
    return IsRR ? E.Opcode32RR : E.Opcode32RI;
  case 8:
    return IsRR ? E.Opcode8RR : E.Opcode8RI;
  default: // 16
    return IsRR ? E.Opcode16RR : E.Opcode16RI;
  }
}

//===------------------------------------------------------------------===//
// Register parsing
//===------------------------------------------------------------------===//

MCRegister ETCAAsmParser::matchRegisterName(StringRef Name) {
  if (Name.empty())
    return MCRegister::NoRegister;

  // Check for ABI register names (binutils-compatible).
  // These must be checked before the size-suffix logic below.
  auto matchABI = [](StringRef N) -> MCRegister {
    return StringSwitch<MCRegister>(N)
        .Case("a0", ETCA::R0)
        .Case("a0x", ETCA::R0)
        .Case("a0h", ETCA::R0)
        .Case("a0d", ETCA::D0)
        .Case("a0q", ETCA::Q0)
        .Case("a1", ETCA::R1)
        .Case("a1x", ETCA::R1)
        .Case("a1h", ETCA::R1)
        .Case("a1d", ETCA::D1)
        .Case("a1q", ETCA::Q1)
        .Case("a2", ETCA::R2)
        .Case("a2x", ETCA::R2)
        .Case("a2h", ETCA::R2)
        .Case("a2d", ETCA::D2)
        .Case("a2q", ETCA::Q2)
        .Case("s0", ETCA::R3)
        .Case("s0x", ETCA::R3)
        .Case("s0h", ETCA::R3)
        .Case("s0d", ETCA::D3)
        .Case("s0q", ETCA::Q3)
        .Case("s1", ETCA::R4)
        .Case("s1x", ETCA::R4)
        .Case("s1h", ETCA::R4)
        .Case("s1d", ETCA::D4)
        .Case("s1q", ETCA::Q4)
        .Case("bp", ETCA::R5)
        .Case("bpx", ETCA::R5)
        .Case("bph", ETCA::R5)
        .Case("bpd", ETCA::D5)
        .Case("bpq", ETCA::Q5)
        .Case("sp", ETCA::R6)
        .Case("spx", ETCA::R6)
        .Case("sph", ETCA::R6)
        .Case("spd", ETCA::D6)
        .Case("spq", ETCA::Q6)
        .Case("ln", ETCA::R7)
        .Case("lnx", ETCA::R7)
        .Case("lnh", ETCA::R7)
        .Case("lnd", ETCA::D7)
        .Case("lnq", ETCA::Q7)
        // REX ABI names: t0-t4 (temps), s2-s4 (callee-saved)
        .Case("t0", ETCA::R8)
        .Case("t0x", ETCA::R8)
        .Case("t0h", ETCA::R8)
        .Case("t0d", ETCA::D8)
        .Case("t0q", ETCA::Q8)
        .Case("t1", ETCA::R9)
        .Case("t1x", ETCA::R9)
        .Case("t1h", ETCA::R9)
        .Case("t1d", ETCA::D9)
        .Case("t1q", ETCA::Q9)
        .Case("t2", ETCA::R10)
        .Case("t2x", ETCA::R10)
        .Case("t2h", ETCA::R10)
        .Case("t2d", ETCA::D10)
        .Case("t2q", ETCA::Q10)
        .Case("t3", ETCA::R11)
        .Case("t3x", ETCA::R11)
        .Case("t3h", ETCA::R11)
        .Case("t3d", ETCA::D11)
        .Case("t3q", ETCA::Q11)
        .Case("t4", ETCA::R12)
        .Case("t4x", ETCA::R12)
        .Case("t4h", ETCA::R12)
        .Case("t4d", ETCA::D12)
        .Case("t4q", ETCA::Q12)
        .Case("s2", ETCA::R13)
        .Case("s2x", ETCA::R13)
        .Case("s2h", ETCA::R13)
        .Case("s2d", ETCA::D13)
        .Case("s2q", ETCA::Q13)
        .Case("s3", ETCA::R14)
        .Case("s3x", ETCA::R14)
        .Case("s3h", ETCA::R14)
        .Case("s3d", ETCA::D14)
        .Case("s3q", ETCA::Q14)
        .Case("s4", ETCA::R15)
        .Case("s4x", ETCA::R15)
        .Case("s4h", ETCA::R15)
        .Case("s4d", ETCA::D15)
        .Case("s4q", ETCA::Q15)
        .Default(MCRegister::NoRegister);
  };

  MCRegister ABI = matchABI(Name);
  if (ABI.isValid())
    return ABI;

  // Check for binutils-style size suffixes.
  // Two formats:
  //   Postfix: rNd (32-bit), rNq (64-bit), rNx (16-bit), rNh (8-bit)
  //   Infix:   rdN (32-bit), rqN (64-bit), rxN (16-bit), rhN (8-bit)
  unsigned RegNum;
  char LastChar = tolower(Name.back());
  bool HasSizeSuffix = (Name.size() >= 3 && tolower(Name[0]) == 'r' &&
                        (LastChar == 'd' || LastChar == 'q' ||
                         LastChar == 'x' || LastChar == 'h'));
  // Check postfix format: rNd, rNq, rNx, rNh
  if (HasSizeSuffix) {
    StringRef NumPart = Name.substr(1, Name.size() - 2);
    if (!NumPart.getAsInteger(10, RegNum) && RegNum <= 15) {
      switch (LastChar) {
      case 'd':
        return ETCA::D0 + RegNum;
      case 'q':
        return ETCA::Q0 + RegNum;
      case 'x':
      case 'h':
        return ETCA::R0 + RegNum;
      }
    }
  }
  // Check infix format: rdN, rqN, rxN, rhN (binutils-compatible)
  if (Name.size() >= 3 && tolower(Name[0]) == 'r') {
    char SecondChar = tolower(Name[1]);
    if (SecondChar == 'd' || SecondChar == 'q' || SecondChar == 'x' ||
        SecondChar == 'h') {
      StringRef NumPart = Name.substr(2);
      if (!NumPart.getAsInteger(10, RegNum) && RegNum <= 15) {
        switch (SecondChar) {
        case 'd':
          return ETCA::D0 + RegNum; // rdN → 32-bit
        case 'q':
          return ETCA::Q0 + RegNum; // rqN → 64-bit
        case 'x':
        case 'h':
          return ETCA::R0 + RegNum; // rxN/rhN → 16-bit/8-bit, R reg
        }
      }
    }
  }

  // Backward-compatible names: rN (16-bit), dN (32-bit), qN (64-bit).
  if (Name.size() < 2)
    return MCRegister::NoRegister;

  char Prefix = tolower(Name[0]);
  if (Prefix != 'r' && Prefix != 'd' && Prefix != 'q')
    return MCRegister::NoRegister;

  if (Name.substr(1).getAsInteger(10, RegNum) || RegNum > 15)
    return MCRegister::NoRegister;

  switch (Prefix) {
  case 'r':
    return ETCA::R0 + RegNum; // rN → 16-bit (default when no suffix)
  case 'd':
    return ETCA::D0 + RegNum; // dN → 32-bit (backward compat)
  case 'q':
    return ETCA::Q0 + RegNum; // qN → 64-bit (backward compat)
  default:
    return MCRegister::NoRegister;
  }
}

unsigned ETCAAsmParser::getWidthHintFromName(StringRef Name) {
  if (Name.empty())
    return 16;
  // Check for postfix size suffixes: rNd, rNq, rNx, rNh
  char LastChar = tolower(Name.back());
  if (Name.size() >= 3 && tolower(Name[0]) == 'r' &&
      (LastChar == 'd' || LastChar == 'q' || LastChar == 'x' ||
       LastChar == 'h')) {
    switch (LastChar) {
    case 'q':
      return 64;
    case 'd':
      return 32;
    case 'h':
      return 8;
    case 'x':
      return 16;
    }
  }
  // Check for infix size suffixes: rdN, rqN, rxN, rhN (binutils-compatible)
  if (Name.size() >= 3 && tolower(Name[0]) == 'r') {
    char SecondChar = tolower(Name[1]);
    if (SecondChar == 'd' || SecondChar == 'q' || SecondChar == 'x' ||
        SecondChar == 'h') {
      switch (SecondChar) {
      case 'q':
        return 64;
      case 'd':
        return 32;
      case 'h':
        return 8;
      case 'x':
        return 16;
      }
    }
  }
  // Check for backward-compatible prefixes.
  char Prefix = tolower(Name[0]);
  if (Prefix == 'd')
    return 32;
  if (Prefix == 'q')
    return 64;
  // ABI names with size suffixes — check last character.
  if (!Name.empty()) {
    char Last = tolower(Name.back());
    if (Last == 'q')
      return 64;
    if (Last == 'd')
      return 32;
    if (Last == 'h')
      return 8;
  }
  return 16; // default
}

bool ETCAAsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                  SMLoc &EndLoc) {
  const AsmToken &Tok = Parser.getTok();
  StartLoc = Tok.getLoc();

  // Handle optional '%' prefix (binutils-compatible register syntax).
  if (Tok.is(AsmToken::Percent)) {
    Parser.Lex(); // consume '%'
    const AsmToken &NameTok = Parser.getTok();
    if (!NameTok.is(AsmToken::Identifier))
      return Error(NameTok.getLoc(), "expected register name after '%'");
    StringRef Name = NameTok.getString();
    Reg = matchRegisterName(Name);
    if (!Reg.isValid())
      return Error(NameTok.getLoc(),
                   "invalid register name, expected rN, rNd, rNq, dN, or qN");
    EndLoc = NameTok.getEndLoc();
    Parser.Lex(); // consume register name
    return false;
  }

  // Without '%' prefix.
  if (!Tok.is(AsmToken::Identifier))
    return Error(StartLoc, "expected register name");

  StringRef Name = Tok.getString();
  Reg = matchRegisterName(Name);
  if (!Reg.isValid())
    return Error(StartLoc,
                 "invalid register name, expected rN, rNd, rNq, dN, or qN");

  EndLoc = Tok.getEndLoc();
  Parser.Lex(); // consume
  return false;
}

ParseStatus ETCAAsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                            SMLoc &EndLoc) {
  const AsmToken &Tok = Parser.getTok();
  StartLoc = Tok.getLoc();

  // Handle optional '%' prefix.
  if (Tok.is(AsmToken::Percent)) {
    Parser.Lex(); // consume '%'
    const AsmToken &NameTok = Parser.getTok();
    if (!NameTok.is(AsmToken::Identifier))
      return ParseStatus::NoMatch;
    StringRef Name = NameTok.getString();
    Reg = matchRegisterName(Name);
    if (!Reg.isValid())
      return ParseStatus::NoMatch;
    EndLoc = NameTok.getEndLoc();
    Parser.Lex(); // consume register name
    return ParseStatus::Success;
  }

  // Without '%' prefix.
  if (!Tok.is(AsmToken::Identifier))
    return ParseStatus::NoMatch;

  StringRef Name = Tok.getString();
  Reg = matchRegisterName(Name);
  if (!Reg.isValid())
    return ParseStatus::NoMatch;

  EndLoc = Tok.getEndLoc();
  Parser.Lex();
  return ParseStatus::Success;
}

//===------------------------------------------------------------------===//
// Operand parsing
//===------------------------------------------------------------------===//

bool ETCAAsmParser::parseOperand(OperandVector &Operands) {
  const AsmToken &Tok = Parser.getTok();
  SMLoc S = Tok.getLoc();

  // Handle bracket memory operand: [reg]
  if (Tok.is(AsmToken::LBrac)) {
    Parser.Lex(); // consume '['
    if (Parser.getTok().is(AsmToken::Percent))
      Parser.Lex(); // consume optional '%'
    const AsmToken &RegTok = Parser.getTok();
    if (!RegTok.is(AsmToken::Identifier))
      return Error(RegTok.getLoc(), "expected register name inside '[]'");
    StringRef RegName = RegTok.getString();
    MCRegister Reg = matchRegisterName(RegName);
    if (!Reg.isValid())
      return Error(RegTok.getLoc(), "invalid register inside '[]'");
    SMLoc E = RegTok.getEndLoc();
    Parser.Lex(); // consume register name
    if (!Parser.getTok().is(AsmToken::RBrac))
      return Error(Parser.getTok().getLoc(), "expected ']' after register");
    Parser.Lex(); // consume ']'
    unsigned WH = getWidthHintFromName(RegName);
    Operands.push_back(ETCAOperand::createMem(Reg, WH, S, E));
    return false;
  }

  // Handle '%' prefix for register names (binutils-compatible syntax).
  if (Tok.is(AsmToken::Percent)) {
    // '%' followed by identifier = register
    Parser.Lex(); // consume '%'
    const AsmToken &NameTok = Parser.getTok();
    if (NameTok.is(AsmToken::Identifier)) {
      StringRef Name = NameTok.getString();
      MCRegister Reg = matchRegisterName(Name);
      if (Reg.isValid()) {
        SMLoc E = NameTok.getEndLoc();
        Parser.Lex(); // consume register name
        unsigned WH = getWidthHintFromName(Name);
        Operands.push_back(ETCAOperand::createReg(Reg, WH, S, E));
        return false;
      }
    }
    // '%' not followed by a valid register — let parseExpression handle it.
    // Put the '%' back by recreating the expression from the current state.
    // Actually, we can't push back, so re-parse as expression.
    const MCExpr *Expr;
    if (Parser.parseExpression(Expr))
      return true;
    SMLoc E = Parser.getTok().getLoc();
    if (const auto *CE = dyn_cast<MCConstantExpr>(Expr))
      Operands.push_back(ETCAOperand::createImm(CE->getValue(), S, E));
    else
      Operands.push_back(ETCAOperand::createExpr(Expr, S, E));
    return false;
  }

  if (Tok.is(AsmToken::Identifier)) {
    StringRef Name = Tok.getString();
    MCRegister Reg = matchRegisterName(Name);
    if (Reg.isValid()) {
      // Register operand
      SMLoc E = Tok.getEndLoc();
      Parser.Lex();
      unsigned WH = getWidthHintFromName(Name);
      Operands.push_back(ETCAOperand::createReg(Reg, WH, S, E));
      return false;
    }
    // Not a register — parse as symbolic expression.
    const MCExpr *Expr;
    if (Parser.parseExpression(Expr))
      return true;
    SMLoc E = Parser.getTok().getLoc();
    // Collapse absolute expressions to immediates.
    if (const auto *CE = dyn_cast<MCConstantExpr>(Expr))
      Operands.push_back(ETCAOperand::createImm(CE->getValue(), S, E));
    else
      Operands.push_back(ETCAOperand::createExpr(Expr, S, E));
    return false;
  }

  // Numeric literal or other expression start.
  const MCExpr *Expr;
  if (Parser.parseExpression(Expr))
    return true;
  SMLoc E = Parser.getTok().getLoc();
  if (const auto *CE = dyn_cast<MCConstantExpr>(Expr))
    Operands.push_back(ETCAOperand::createImm(CE->getValue(), S, E));
  else
    Operands.push_back(ETCAOperand::createExpr(Expr, S, E));
  return false;
}

//===------------------------------------------------------------------===//
// Instruction parsing
//===------------------------------------------------------------------===//

bool ETCAAsmParser::parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                                     SMLoc NameLoc, OperandVector &Operands) {
  // Detect and strip instruction size suffix (binutils-compatible).
  // Suffixes: h=byte, x=word, d=dword, q=qword
  // E.g., addd → width=32, base=add; loadq → width=64, base=load
  CurrentInstrWidth = 0;
  StringRef BaseName = Name;
  if (Name.size() >= 3) { // at least 2 chars + suffix
    char Last = tolower(Name.back());
    if (Last == 'h' || Last == 'x' || Last == 'd' || Last == 'q') {
      // Check that the remaining prefix is a known instruction base.
      BaseName = Name.drop_back(1);
      // Verify it's a valid base by trying the lookups.
      if (lookupALU(BaseName) || lookupBranch(BaseName) ||
          lookupFixed(BaseName)) {
        switch (Last) {
        case 'q':
          CurrentInstrWidth = 64;
          break;
        case 'd':
          CurrentInstrWidth = 32;
          break;
        case 'h':
          CurrentInstrWidth = 8;
          break;
        case 'x':
          CurrentInstrWidth = 16;
          break;
        }
      } else {
        // Not a valid base with suffix, use full name.
        BaseName = Name;
      }
    }
  }

  // Save the base mnemonic for use in matchAndEmitInstruction.
  CurrentMnemonic = BaseName.lower();

  // Push the mnemonic as a token (operand 0).
  Operands.push_back(ETCAOperand::createTok(NameLoc, NameLoc));

  // Parse operands if any.
  if (Parser.getTok().isNot(AsmToken::EndOfStatement)) {
    // First operand (no leading comma).
    if (parseOperand(Operands))
      return true;

    // Additional operands (comma-separated).
    while (Parser.getTok().is(AsmToken::Comma)) {
      Parser.Lex(); // eat comma
      if (parseOperand(Operands))
        return true;
    }
  }

  if (Parser.getTok().isNot(AsmToken::EndOfStatement))
    return Error(Parser.getTok().getLoc(), "unexpected token in operand list");

  return false;
}

//===------------------------------------------------------------------===//
// Instruction matching and emission
//===------------------------------------------------------------------===//

bool ETCAAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                            OperandVector &Operands,
                                            MCStreamer &Out,
                                            uint64_t &ErrorInfo,
                                            bool MatchingInlineAsm) {
  StringRef Mnemonic = CurrentMnemonic;

  // Operands[0] = mnemonic token; Operands[1..] = actual operands.
  size_t NumOps = Operands.size() - 1;

  // ---- NOP ----
  if (Mnemonic == "nop") {
    MCInst Inst;
    Inst.setOpcode(ETCA::NOP);
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, getSTI());
    return false;
  }

  // ---- Pseudo-instructions (CodeGenOnly — shouldn't appear in user asm) ----
  if (Mnemonic == "ret" || Mnemonic == "select") {
    return Error(IDLoc, "pseudo-instruction '" + Mnemonic +
                            "' is not valid in assembly");
  }

  // ---- Branch instructions ----
  unsigned BrOpc = lookupBranch(Mnemonic);
  if (BrOpc) {
    if (NumOps != 1)
      return Error(IDLoc, "branch instruction expects 1 operand (target)");
    auto &Op = static_cast<ETCAOperand &>(*Operands[1]);
    MCInst Inst;
    Inst.setOpcode(BrOpc);
    Inst.setLoc(IDLoc);
    if (Op.isImm()) {
      // Resolved immediate displacement.
      Inst.addOperand(MCOperand::createImm(Op.getImm()));
    } else if (Op.isExpr()) {
      Inst.addOperand(MCOperand::createExpr(Op.getExpr()));
    } else {
      return Error(IDLoc, "branch target must be an immediate or label");
    }
    Out.emitInstruction(Inst, getSTI());
    return false;
  }

  // ---- Fixed-format instructions (LOAD, STORE, SAF ops) ----
  const FixedEntry *FE = lookupFixed(Mnemonic);
  if (FE) {
    if (FE->IsMov) {
      // MOV pseudo-instruction: handles multiple forms.
      // Must be checked BEFORE NumOps==0 (mov has NumOps=0 to bypass the count
      // check).
      //   mov dst, src_reg     → movs dst, src_reg
      //   mov dst, imm         → movs dst, imm
      //   mov [addr], src_reg  → store addr, src_reg
      //   mov dst_reg, [addr]  → load dst_reg, addr
      if (NumOps < 1 || NumOps > 2)
        return Error(IDLoc, "mov expects 1 or 2 operands");

      MCInst Inst;
      Inst.setLoc(IDLoc);

      unsigned Width = CurrentInstrWidth;
      auto &Op1 = static_cast<ETCAOperand &>(*Operands[1]);
      bool Op1IsMem = Op1.isMem();
      bool Op2IsMem =
          (NumOps >= 2) && static_cast<ETCAOperand &>(*Operands[2]).isMem();

      if (Op1IsMem && NumOps >= 2) {
        auto &Op2 = static_cast<ETCAOperand &>(*Operands[2]);
        if (!Op2.isReg())
          return Error(IDLoc, "mov [addr], src: src must be a register");
        unsigned W = Width ? Width : Op1.getMemWidthHint();
        switch (W) {
        case 8:
          Inst.setOpcode(ETCA::STORE8);
          break;
        case 32:
          Inst.setOpcode(ETCA::STORE32);
          break;
        case 64:
          Inst.setOpcode(ETCA::STORE64);
          break;
        default:
          Inst.setOpcode(ETCA::STORE16);
          break;
        }
        Inst.addOperand(MCOperand::createReg(Op2.getReg()));
        Inst.addOperand(MCOperand::createReg(Op1.getMemReg()));
      } else if (Op2IsMem) {
        auto &Op2 = static_cast<ETCAOperand &>(*Operands[2]);
        if (!Op1.isReg())
          return Error(IDLoc, "mov dst, [addr]: dst must be a register");
        unsigned W = Width ? Width : Op2.getMemWidthHint();
        switch (W) {
        case 8:
          Inst.setOpcode(ETCA::LOAD8);
          break;
        case 32:
          Inst.setOpcode(ETCA::LOAD32);
          break;
        case 64:
          Inst.setOpcode(ETCA::LOAD64);
          break;
        default:
          Inst.setOpcode(ETCA::LOAD16);
          break;
        }
        Inst.addOperand(MCOperand::createReg(Op1.getReg()));
        Inst.addOperand(MCOperand::createReg(Op2.getMemReg()));
      } else if (NumOps == 2) {
        if (!Op1.isReg())
          return Error(IDLoc, "mov: first operand must be a register");
        unsigned W = Width ? Width : Op1.getRegisterWidth();
        auto &Op2 = static_cast<ETCAOperand &>(*Operands[2]);
        if (Op2.isReg()) {
          switch (W) {
          case 8:
            Inst.setOpcode(ETCA::MOVZ8);
            break;
          case 32:
            Inst.setOpcode(ETCA::MOVZ32);
            break;
          case 64:
            Inst.setOpcode(ETCA::MOVZ64);
            break;
          default:
            Inst.setOpcode(ETCA::MOVZ16);
            break;
          }
          Inst.addOperand(MCOperand::createReg(Op1.getReg()));
          Inst.addOperand(MCOperand::createReg(Op2.getReg()));
        } else if (Op2.isImm()) {
          int64_t Val = Op2.getImm();
          unsigned RegNum = Op1.getReg();
          // For 16-bit width, split large immediates into MOVZ + SLO chain.
          if (W == 16 && (Val < 0 || Val > 31)) {
            // Split into 5-bit chunks (LSB first).
            // Use unsigned value for chunking.
            uint64_t UV = static_cast<uint64_t>(static_cast<int64_t>(Val));
            UV &= (1ULL << W) - 1; // mask to width
            // Collect 5-bit chunks from LSB to MSB
            SmallVector<unsigned, 4> Chunks;
            while (UV) {
              Chunks.push_back(UV & 0x1F);
              UV >>= 5;
            }
            if (Chunks.empty()) {
              // Val is 0 — single MOVZ
              Inst.setOpcode(ETCA::MOVZI16);
              Inst.addOperand(MCOperand::createReg(RegNum));
              Inst.addOperand(MCOperand::createImm(0));
            } else {
              // First: MOVZ with highest chunk
              Inst.setOpcode(ETCA::MOVZI16);
              Inst.addOperand(MCOperand::createReg(RegNum));
              Inst.addOperand(MCOperand::createImm(Chunks.back()));
              Chunks.pop_back();
              Out.emitInstruction(Inst, getSTI());
              // Remaining chunks: SLO
              while (!Chunks.empty()) {
                Inst = MCInst();
                Inst.setLoc(IDLoc);
                Inst.setOpcode(ETCA::SLO16);
                Inst.addOperand(MCOperand::createReg(RegNum));
                Inst.addOperand(MCOperand::createImm(Chunks.back()));
                Chunks.pop_back();
                Out.emitInstruction(Inst, getSTI());
              }
              Inst = MCInst(); // Clear so we don't emit again below
            }
          } else {
            // Fits in 5-bit immediate, or non-16-bit width: use direct
            // MOVS/MOVZ
            switch (W) {
            case 8:
              Inst.setOpcode(ETCA::MOVZI8);
              break;
            case 32:
              Inst.setOpcode(ETCA::MOVZI32);
              break;
            case 64:
              Inst.setOpcode(ETCA::MOVZI64);
              break;
            default:
              Inst.setOpcode(ETCA::MOVZI16);
              break;
            }
            Inst.addOperand(MCOperand::createReg(RegNum));
            Inst.addOperand(MCOperand::createImm(Val & 0x1F));
          }
        } else {
          return Error(IDLoc,
                       "mov: second operand must be register or immediate");
        }
      } else {
        return Error(IDLoc, "mov expects 1 or 2 operands");
      }

      // Emit the instruction (for SLO chain case, already emitted inside the
      // block).
      if (Inst.getOpcode() != 0)
        Out.emitInstruction(Inst, getSTI());
      return false;
    }

    if (FE->NumOps == 0) {
      // No-operand (NOP)
      MCInst Inst;
      Inst.setOpcode(FE->Opcode);
      Inst.setLoc(IDLoc);
      Out.emitInstruction(Inst, getSTI());
      return false;
    }

    if (NumOps != FE->NumOps)
      return Error(IDLoc,
                   "instruction expects " + Twine(FE->NumOps) + " operand(s)");

    MCInst Inst;
    Inst.setOpcode(FE->Opcode);
    Inst.setLoc(IDLoc);
    Inst.setLoc(IDLoc);
    if (FE->IsTargetExpr) {
      // CALL: operand is a target expression.
      auto &Op = static_cast<ETCAOperand &>(*Operands[1]);
      if (Op.isImm())
        Inst.addOperand(MCOperand::createImm(Op.getImm()));
      else if (Op.isExpr())
        Inst.addOperand(MCOperand::createExpr(Op.getExpr()));
      else
        return Error(IDLoc, "call target must be an immediate or label");
    } else if (FE->Opcode == ETCA::LOAD16 || FE->Opcode == ETCA::STORE16) {
      // LOAD/STORE are width-aware. Prefer instruction suffix width;
      // fall back to first operand's register width.
      auto &Op1 = static_cast<ETCAOperand &>(*Operands[1]);
      if (!Op1.isReg())
        return Error(IDLoc, "first operand must be a register");
      unsigned Width =
          CurrentInstrWidth ? CurrentInstrWidth : Op1.getRegisterWidth();
      bool IsLoad = (FE->Opcode == ETCA::LOAD16);
      switch (Width) {
      case 8:
        Inst.setOpcode(IsLoad ? ETCA::LOAD8 : ETCA::STORE8);
        break;
      case 32:
        Inst.setOpcode(IsLoad ? ETCA::LOAD32 : ETCA::STORE32);
        break;
      case 64:
        Inst.setOpcode(IsLoad ? ETCA::LOAD64 : ETCA::STORE64);
        break;
      default:
        Inst.setOpcode(IsLoad ? ETCA::LOAD16 : ETCA::STORE16);
        break;
      }
      Inst.addOperand(MCOperand::createReg(Op1.getReg()));
      if (FE->NumOps >= 2) {
        auto &Op2 = static_cast<ETCAOperand &>(*Operands[2]);
        if (!Op2.isReg())
          return Error(IDLoc, "second operand must be a register");
        Inst.addOperand(MCOperand::createReg(Op2.getReg()));
      }
    } else if (FE->Opcode == ETCA::PUSH) {
      // PUSH <reg> or PUSH <imm> — distinguish by operand type.
      auto &Op = static_cast<ETCAOperand &>(*Operands[1]);
      if (Op.isReg()) {
        unsigned Reg = Op.getReg();
        // Select width-specific PUSH based on register class.
        if (Reg >= ETCA::D0 && Reg <= ETCA::D7)
          Inst.setOpcode(ETCA::PUSH32);
        else if (Reg >= ETCA::Q0 && Reg <= ETCA::Q7)
          Inst.setOpcode(ETCA::PUSH64);
        else
          Inst.setOpcode(ETCA::PUSH);
        Inst.addOperand(MCOperand::createReg(Reg));
      } else if (Op.isImm()) {
        Inst.setOpcode(ETCA::PUSHI);
        Inst.addOperand(MCOperand::createImm(Op.getImm()));
      } else if (Op.isExpr()) {
        Inst.setOpcode(ETCA::PUSHI);
        Inst.addOperand(MCOperand::createExpr(Op.getExpr()));
      } else {
        return Error(IDLoc, "push operand must be register or immediate");
      }

    } else if (FE->Opcode == ETCA::POP) {
      // POP <reg> — select width-specific POP based on register class.
      auto &Op = static_cast<ETCAOperand &>(*Operands[1]);
      if (!Op.isReg())
        return Error(IDLoc, "pop operand must be a register");
      unsigned Reg = Op.getReg();
      if (Reg >= ETCA::D0 && Reg <= ETCA::D7)
        Inst.setOpcode(ETCA::POP32);
      else if (Reg >= ETCA::Q0 && Reg <= ETCA::Q7)
        Inst.setOpcode(ETCA::POP64);
      else
        Inst.setOpcode(ETCA::POP);
      Inst.addOperand(MCOperand::createReg(Reg));

    } else {
      // Register operands: JMPR, CALLR
      for (size_t I = 1; I <= FE->NumOps; ++I) {
        auto &Op = static_cast<ETCAOperand &>(*Operands[I]);
        if (!Op.isReg())
          return Error(IDLoc, "operand " + Twine(I) + " must be a register");
        Inst.addOperand(MCOperand::createReg(Op.getReg()));
      }
    }

    Out.emitInstruction(Inst, getSTI());
    return false;
  }

  // ---- ALU instructions (RR or RI, width-polymorphic) ----
  const RrRiEntry *AE = lookupALU(Mnemonic);
  if (AE) {
    if (NumOps != 2)
      return Error(IDLoc, "instruction expects 2 operands (dst, src2 or imm)");

    auto &Op1 = static_cast<ETCAOperand &>(*Operands[1]); // dst (also src1)
    auto &Op2 = static_cast<ETCAOperand &>(*Operands[2]); // src2 or imm

    if (!Op1.isReg())
      return Error(IDLoc, "first operand must be a register");

    bool IsRR = Op2.isReg();

    // Validate: some ops are RI-only or RR-only.
    if (!IsRR && Op2.isExpr())
      return Error(IDLoc, "second operand must be a register or integer "
                          "immediate (labels not allowed here)");

    // Use instruction suffix width if available; otherwise infer from register.
    unsigned Width =
        CurrentInstrWidth ? CurrentInstrWidth : Op1.getRegisterWidth();
    if (IsRR) {
      unsigned W2 = Op2.getRegisterWidth();
      if (Width != W2)
        return Error(IDLoc, "register width mismatch in operands");
    }
    // If instruction suffix forced a width, validate that register width
    // is compatible (e.g., addd with 32-bit regs).
    if (CurrentInstrWidth && Width != CurrentInstrWidth && IsRR &&
        Op1.getRegisterWidth() != CurrentInstrWidth) {
      return Error(IDLoc,
                   "instruction width suffix does not match register width");
    }

    unsigned Opc = selectWidth(*AE, IsRR, Width);
    if (!Opc)
      return Error(IDLoc, "instruction not available in this width/form");

    MCInst Inst;
    Inst.setOpcode(Opc);
    Inst.setLoc(IDLoc);

    // CMP and TEST RR form (dedicated format) have NO output — 2 operands.
    // CMPI and TESTI RI form follow the EInstRI template with $dst output
    // and $src1=$dst constraint — 3 operands like other ALU ops.
    // MOVZ/MOVS RR and MOVZI/MOVSI RI use non-tied formats (EInstRR_NT /
    // EInstRI_NT) with only 2 operands: [dst, src] for RR, [dst, imm] for RI.
    // All other ALU ops use the tied format with 3 operands: [dst, src1,
    // src2/imm].
    bool IsMovRR =
        (Opc == ETCA::MOVZ8 || Opc == ETCA::MOVS8 || Opc == ETCA::MOVZ16 ||
         Opc == ETCA::MOVS16 || Opc == ETCA::MOVZ32 || Opc == ETCA::MOVS32 ||
         Opc == ETCA::MOVZ64 || Opc == ETCA::MOVS64);
    bool IsMovRI =
        (Opc == ETCA::MOVZI8 || Opc == ETCA::MOVSI8 || Opc == ETCA::MOVZI16 ||
         Opc == ETCA::MOVSI16 || Opc == ETCA::MOVZI32 || Opc == ETCA::MOVSI32 ||
         Opc == ETCA::MOVZI64 || Opc == ETCA::MOVSI64);
    bool IsSloRI = (Opc == ETCA::SLO16);
    bool IsCmpRR =
        (Opc == ETCA::CMP8 || Opc == ETCA::CMP || Opc == ETCA::CMP32 ||
         Opc == ETCA::CMP64 || Opc == ETCA::TEST8 || Opc == ETCA::TEST ||
         Opc == ETCA::TEST32 || Opc == ETCA::TEST64);

    if (IsCmpRR) {
      // 2-operand layout for CMP/TEST RR: [src1, src2]
      Inst.addOperand(MCOperand::createReg(Op1.getReg())); // src1
      Inst.addOperand(MCOperand::createReg(Op2.getReg())); // src2
    } else if (IsMovRR) {
      // 2-operand layout for MOVZ/MOVS RR: [dst, src]
      Inst.addOperand(MCOperand::createReg(Op1.getReg())); // dst
      Inst.addOperand(MCOperand::createReg(Op2.getReg())); // src
    } else if (IsMovRI || IsSloRI) {
      // 2-operand layout for MOVZI/MOVSI RI: [dst, imm]
      Inst.addOperand(MCOperand::createReg(Op1.getReg())); // dst
      if (Op2.isImm())
        Inst.addOperand(MCOperand::createImm(Op2.getImm()));
      else
        Inst.addOperand(MCOperand::createExpr(Op2.getExpr()));
    } else {
      // Standard 3-operand layout for tied ALU ops: [dst, src1, src2/imm]
      // CMPI/TESTI fall here: they have a $dst in AsmString.
      Inst.addOperand(MCOperand::createReg(Op1.getReg())); // dst
      Inst.addOperand(MCOperand::createReg(Op1.getReg())); // src1 = dst
      if (IsRR) {
        Inst.addOperand(MCOperand::createReg(Op2.getReg())); // src2
      } else {
        if (Op2.isImm())
          Inst.addOperand(MCOperand::createImm(Op2.getImm()));
        else
          Inst.addOperand(MCOperand::createExpr(Op2.getExpr()));
      }
    }

    Out.emitInstruction(Inst, getSTI());
    return false;
  }

  // ---- Unknown mnemonic ----
  return Error(IDLoc, "unknown instruction mnemonic '" + Mnemonic + "'");
}

//===------------------------------------------------------------------===//
// LLVM initialization
//===------------------------------------------------------------------===//

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeETCAAsmParser() {
  RegisterMCAsmParser<ETCAAsmParser> X(getTheETCATarget());
}
