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
  // Determine pointer size from the triple's OS name suffix:
  //   etca-unknown-elf    → 16-bit (base ISA, default)
  //   etca-unknown-elf32  → 32-bit (DWAS extension)
  //   etca-unknown-elf64  → 64-bit (QWAS extension)
  //
  // This is the mechanism by which the MC layer learns the pointer width,
  // since MCAsmInfo is created before MCSubtargetInfo is available and
  // ETCa uses a single Triple::etca architecture for all pointer sizes.
  // The object format is correctly set to ELF via getDefaultFormat()
  // regardless of the OS suffix.
  unsigned PtrSize = 16;
  StringRef OSName = TT.getOSName();
  if (OSName == "elf32")
    PtrSize = 32;
  else if (OSName == "elf64")
    PtrSize = 64;

  return new ETCAMCAsmInfo(TT, Options, PtrSize);
}

//===----------------------------------------------------------------------===//
// ETCA MCCodeEmitter
//===----------------------------------------------------------------------===//
//
// ETCA instruction encoding uses the auto-generated emitter from
// ETCAGenMCCodeEmitter.inc.  The TableGen format classes in
// ETCAInstrFormats.td define the Inst field to match the actual
// 16-bit little-endian encoding:
//
//   RR format (Byte 0: 0b00SSCCCC, Byte 1: 0bAAABBBMM)
//   RI format (Byte 0: 0b01SSCCCC, Byte 1: 0bAAAIIIII)
//
// All instructions are 16-bit fixed width, little-endian.
//
//===----------------------------------------------------------------------===//

namespace {

class ETCAMCCodeEmitter : public MCCodeEmitter {
  const MCInstrInfo &MCII;
  MCContext &Ctx;

public:
  ETCAMCCodeEmitter(const MCInstrInfo &MII, MCContext &Ctx)
      : MCII(MII), Ctx(Ctx) {
    (void)MCII;
  }

  /// Emit a MOV_* spanning chain (MOVZI + SLOs) for an address materialization.
  /// Called from encodeInstruction for JT_Pseudo, or when MOVZI/MOVS has a
  /// label expression operand.
  void emitMovChain(unsigned Opcode, unsigned DstReg, const MCOperand &LabelOp,
                    SmallVectorImpl<char> &CB, SmallVectorImpl<MCFixup> &Fixups,
                    const MCSubtargetInfo &STI) const;

  /// Return true if this is a MOVZI/MOVS instruction variant.
  static bool isMovImmOpcode(unsigned Opcode);

  /// Return the SS field from a MOVZI/MOVS opcode.
  static unsigned getMovSS(unsigned Opcode);

  /// Emit a MOV_* spanning chain (MOVZI + SLOs) for a jump table address.
  void emitJTChain(const MCInst &MI, SmallVectorImpl<char> &CB,
                   SmallVectorImpl<MCFixup> &Fixups,
                   const MCSubtargetInfo &STI) const;

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

  /// Auto-generated by TableGen from ETCAInstrFormats.td.
  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

private:
  /// Return the register number (0-7) from a register operand.
  unsigned getRegisterOpValue(const MCInst &MI, unsigned OpNo) const;

  /// Called by the auto-generated emitter for operands without an
  /// explicit EncoderMethod.  Handles registers, immediates, and expressions.
  unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  /// Return the immediate value from an operand, or 0 if it's an expression.
  /// If the operand is an MCExpr, records a fixup and returns 0.
  unsigned getImmOpValue(const MCInst &MI, unsigned OpNo,
                         SmallVectorImpl<MCFixup> &Fixups,
                         MCFixupKind FixupKind) const;

  /// Branch target encoding: 9-bit signed displacement.
  /// Signature matches what the auto-generated emitter expects.
  unsigned encodeBranchTarget(const MCInst &MI, unsigned OpNo,
                              SmallVectorImpl<MCFixup> &Fixups,
                              const MCSubtargetInfo &STI) const;

  /// CALL target encoding: 12-bit signed displacement.
  unsigned encodeCallTarget(const MCInst &MI, unsigned OpNo,
                            SmallVectorImpl<MCFixup> &Fixups,
                            const MCSubtargetInfo &STI) const;

  /// Compute REX prefix byte (0xC0 | flags) from register operands.
  /// Returns 0 if no REX prefix is needed.
  unsigned computeRexPrefix(const MCInst &MI) const;
};

} // namespace

unsigned ETCAMCCodeEmitter::getRegisterOpValue(const MCInst &MI,
                                               unsigned OpNo) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  assert(MO.isReg() && "Expected register operand");
  // Register encoding: use the HWEncoding from the target register info.
  // All register classes (R0-R7, D0-D7, Q0-Q7) share the same 3-bit encoding
  // for the same index (e.g., R0/D0/Q0 all encode to 0).
  return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg()) & 0x7;
}

unsigned
ETCAMCCodeEmitter::getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                                     SmallVectorImpl<MCFixup> &Fixups,
                                     const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg()) & 0x7;
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  if (MO.isExpr()) {
    // Expression operands should use custom encoder methods (e.g.,
    // encodeBranchTarget, encodeCallTarget) which create fixups.
    // If an Expr reaches here without a custom encoder, return 0.
    return 0;
  }
  return 0;
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
                                      SmallVectorImpl<MCFixup> &Fixups,
                                      const MCSubtargetInfo &STI) const {
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

unsigned ETCAMCCodeEmitter::encodeCallTarget(const MCInst &MI, unsigned OpNo,
                                             SmallVectorImpl<MCFixup> &Fixups,
                                             const MCSubtargetInfo &STI) const {
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

unsigned ETCAMCCodeEmitter::computeRexPrefix(const MCInst &MI) const {
  // Compute the REX prefix byte from register operands (HWEncoding bit 3).
  // REX byte: 0xC0 | Q<<3 | A<<2 | B<<1 | X
  //   A = bit 3 of AAA field (first register operand)
  //   B = bit 3 of BBB field (second register operand, or PUSH source)
  //   Q, X = 0 for base ISA (used by FI/MO1/MO2 extensions)
  unsigned RexA = 0, RexB = 0;
  unsigned Opc = MI.getOpcode();

  // PUSH/PUSH32/PUSH64: the single register operand is in the BBB field
  // (bits 12-10), NOT the AAA field.  AAA is hardwired to sp (6) in the
  // instruction encoding.
  bool IsPUSH = (Opc == ETCA::PUSH || Opc == ETCA::PUSH8 ||
                 Opc == ETCA::PUSH32 || Opc == ETCA::PUSH64);

  unsigned RegIdx = 0;
  for (unsigned i = 0; i < MI.getNumOperands(); ++i) {
    if (!MI.getOperand(i).isReg())
      continue;
    unsigned HWEnc =
        Ctx.getRegisterInfo()->getEncodingValue(MI.getOperand(i).getReg());
    unsigned RexBit = (HWEnc >> 3) & 1;
    if (RexBit == 0) {
      ++RegIdx;
      continue;
    }

    if (IsPUSH && i == 0) {
      // PUSH: single register operand maps to BBB
      RexB |= 1;
    } else if (RegIdx == 0) {
      // First register operand → AAA field → REX.A
      RexA |= 1;
    } else {
      // Second register operand → BBB field → REX.B
      RexB |= 1;
    }
    ++RegIdx;
  }

  unsigned Rex = 0;
  if (RexA || RexB) {
    Rex = 0xC0 | (RexA << 2) | (RexB << 1);
  }
  return Rex;
}

bool ETCAMCCodeEmitter::isMovImmOpcode(unsigned Opcode) {
  switch (Opcode) {
  case ETCA::MOVZI8:
  case ETCA::MOVZI16:
  case ETCA::MOVZI32:
  case ETCA::MOVZI64:
  case ETCA::MOVSI8:
  case ETCA::MOVSI16:
  case ETCA::MOVSI32:
  case ETCA::MOVSI64:
    return true;
  default:
    return false;
  }
}

unsigned ETCAMCCodeEmitter::getMovSS(unsigned Opcode) {
  switch (Opcode) {
  case ETCA::MOVZI8:
  case ETCA::MOVSI8:
    return 0; // byte
  case ETCA::MOVZI32:
  case ETCA::MOVSI32:
    return 2; // dword
  case ETCA::MOVZI64:
  case ETCA::MOVSI64:
    return 3; // qword
  default:
    return 1; // word (16-bit)
  }
}

void ETCAMCCodeEmitter::emitMovChain(unsigned Opcode, unsigned DstReg,
                                     const MCOperand &LabelOp,
                                     SmallVectorImpl<char> &CB,
                                     SmallVectorImpl<MCFixup> &Fixups,
                                     const MCSubtargetInfo &STI) const {
  unsigned RegEnc = Ctx.getRegisterInfo()->getEncodingValue(DstReg) & 0x7;
  unsigned SS = getMovSS(Opcode);

  // Determine MOV_* fixup type and chain size from the SS bits.
  // SS=0 (byte)  → MOV_8  (2 insns, 4 bytes)
  // SS=1 (word)  → MOV_16 (4 insns, 8 bytes)
  // SS=2 (dword) → MOV_32 (7 insns, 14 bytes)
  // SS=3 (qword) → MOV_64 (13 insns, 26 bytes)
  ETCA::Fixups MovFixup;
  unsigned ChainBytes;
  switch (SS) {
  case 0:
    MovFixup = ETCA::fixup_ETCA_MOV_8;
    ChainBytes = 4;
    break;
  case 2:
    MovFixup = ETCA::fixup_ETCA_MOV_32;
    ChainBytes = 14;
    break;
  case 3:
    MovFixup = ETCA::fixup_ETCA_MOV_64;
    ChainBytes = 26;
    break;
  default:
    MovFixup = ETCA::fixup_ETCA_MOV_16;
    ChainBytes = 8;
    break;
  }

  // Determine MOVZI/MOVS opcode byte based on the instruction type.
  // RI format: bits[7:6]=01, bits[5:4]=SS, bits[3:0]=opcode
  //   MOVZI opcode = 8 (0b1000)
  //   MOVSI opcode = 9 (0b1001)
  //   SLO   opcode = 12 (0b1100)
  unsigned FirstOpc;
  switch (Opcode) {
  case ETCA::MOVSI8:
  case ETCA::MOVSI16:
  case ETCA::MOVSI32:
  case ETCA::MOVSI64:
    FirstOpc = 9; // MOVSI
    break;
  default:
    FirstOpc = 8; // MOVZI
    break;
  }

  // Emit placeholder chain.
  unsigned NumInsns = ChainBytes / 2;
  for (unsigned i = 0; i < NumInsns; ++i) {
    uint16_t Inst;
    if (i == 0)
      Inst = 0b01000000 | (SS << 4) | FirstOpc;
    else
      Inst = 0b01000000 | (SS << 4) | 12; // SLO
    Inst |= (RegEnc << 13);               // AAA: destination register
    CB.push_back(static_cast<char>(Inst & 0xFF));
    CB.push_back(static_cast<char>((Inst >> 8) & 0xFF));
  }

  // Create a spanning fixup on the first byte of the chain.
  if (LabelOp.isExpr()) {
    Fixups.push_back(
        MCFixup::create(0, LabelOp.getExpr(), MCFixupKind(MovFixup)));
  }
}

void ETCAMCCodeEmitter::emitJTChain(const MCInst &MI, SmallVectorImpl<char> &CB,
                                    SmallVectorImpl<MCFixup> &Fixups,
                                    const MCSubtargetInfo &STI) const {
  // JT_Pseudo uses pointer-size-dependent chain.
  unsigned DstReg = MI.getOperand(0).getReg();
  const MCOperand &LabelOp = MI.getOperand(1);

  // Determine pointer size from subtarget features.
  unsigned Opcode;
  if (STI.hasFeature(ETCA::FeaturePtr64))
    Opcode = ETCA::MOVZI64;
  else if (STI.hasFeature(ETCA::FeaturePtr32))
    Opcode = ETCA::MOVZI32;
  else
    Opcode = ETCA::MOVZI16;

  emitMovChain(Opcode, DstReg, LabelOp, CB, Fixups, STI);
}

void ETCAMCCodeEmitter::encodeInstruction(const MCInst &MI,
                                          SmallVectorImpl<char> &CB,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  unsigned Opcode = MI.getOpcode();

  // Handle JT_Pseudo specially: emit MOV_* chain with spanning fixup.
  if (Opcode == ETCA::JT_Pseudo) {
    emitJTChain(MI, CB, Fixups, STI);
    return;
  }

  // When MOVZI/MOVS has a label expression as its immediate operand,
  // emit a MOV_* spanning chain so the linker can expand it.
  // This handles assembly like "movz r0, label" from user-written asm.
  if (isMovImmOpcode(Opcode) && MI.getNumOperands() >= 2 &&
      MI.getOperand(1).isExpr()) {
    emitMovChain(Opcode, MI.getOperand(0).getReg(), MI.getOperand(1), CB,
                 Fixups, STI);
    return;
  }

  // Determine if a REX prefix is needed based on register operands.
  unsigned RexByte = computeRexPrefix(MI);

  // Let the auto-generated getBinaryCodeForInstr build the encoding
  // (the Inst field already has the 3-bit register encodings).
  uint64_t Binary = getBinaryCodeForInstr(MI, Fixups, STI);

  // Determine instruction size from the opcode descriptor.
  unsigned Size = MCII.get(MI.getOpcode()).getSize();
  if (Size == 0)
    Size = 2;

  if (RexByte) {
    // Emit REX prefix byte before the instruction.
    // Total size = prefix + instruction.
    CB.push_back(static_cast<char>(RexByte));
  }

  // Write instruction bytes in little-endian order.
  for (unsigned i = 0; i < Size; ++i)
    CB.push_back(static_cast<char>((Binary >> (i * 8)) & 0xFF));
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
  bool Is64Bit;

public:
  ETCAAsmBackend(const MCSubtargetInfo &STI, const MCTargetOptions &Options)
      : MCAsmBackend(llvm::endianness::little), Is64Bit(false) {
    // Use 64-bit ELF when pointers are 64-bit (any CPU model with ptr64).
    if (STI.hasFeature(ETCA::FeaturePtr64))
      Is64Bit = true;
  }

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
      // For MOV_* spanning fixups, write the absolute address as raw bytes
      // (placeholder chain will be replaced by the linker's build_mov_ri).
      if (Kind >= ETCA::fixup_ETCA_MOV_5 && Kind <= ETCA::fixup_ETCA_MOV_32) {
        unsigned Size = getMovChainBytes(static_cast<ETCA::Fixups>(Kind));
        for (unsigned i = 0; i < Size; ++i)
          Data[i] = static_cast<uint8_t>((Value >> (i * 8)) & 0xFF);
        break;
      }
      llvm_unreachable("unknown ETCA fixup kind");
    }
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createETCAELFObjectWriter(0, Is64Bit);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    static const MCFixupKindInfo Infos[] = {
        {"fixup_ETCA_NONE", 0, 0, 0},
        {"fixup_ETCA_BASE_JMP", 0, 16, 0},
        {"fixup_ETCA_SAF_CALL", 0, 16, 0},
        {"fixup_ETCA_8", 0, 8, 0},
        {"fixup_ETCA_16", 0, 16, 0},
        {"fixup_ETCA_32", 0, 32, 0},
        {"fixup_ETCA_64", 0, 64, 0},
        // MOV_* spanning: size is the full chain byte count.
        {"fixup_ETCA_MOV_5", 0, 16, 0},
        {"fixup_ETCA_MOV_10", 0, 16, 0},
        {"fixup_ETCA_MOV_15", 0, 16, 0},
        {"fixup_ETCA_MOV_20", 0, 16, 0},
        {"fixup_ETCA_MOV_25", 0, 16, 0},
        {"fixup_ETCA_MOV_30", 0, 16, 0},
        {"fixup_ETCA_MOV_35", 0, 16, 0},
        {"fixup_ETCA_MOV_40", 0, 16, 0},
        {"fixup_ETCA_MOV_45", 0, 16, 0},
        {"fixup_ETCA_MOV_50", 0, 16, 0},
        {"fixup_ETCA_MOV_55", 0, 16, 0},
        {"fixup_ETCA_MOV_60", 0, 16, 0},
        {"fixup_ETCA_MOV_64", 0, 16, 0},
        {"fixup_ETCA_MOV_8", 0, 16, 0},
        {"fixup_ETCA_MOV_16", 0, 16, 0},
        {"fixup_ETCA_MOV_32", 0, 16, 0},
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
    // NOP = 2-byte NOP (0x008F LE: [0x8F, 0x00]) per binutils etca_build_nop.
    // Write as many full NOPs as the even-aligned portion allows, then return
    // false for the odd remainder so LLVM's generic fallback emits a trap.
    uint64_t NopCount = Count / 2;
    for (uint64_t i = 0; i < NopCount; ++i)
      OS.write("\x8F\x00", 2);
    // If Count is odd, return false -- the caller will handle the last byte.
    return (Count % 2) == 0;
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
  // MOV_* spanning relocations (must match binutils numbering).
  R_ETCA_MOV_5 = 17,
  R_ETCA_MOV_10 = 18,
  R_ETCA_MOV_15 = 19,
  R_ETCA_MOV_20 = 20,
  R_ETCA_MOV_25 = 21,
  R_ETCA_MOV_30 = 22,
  R_ETCA_MOV_35 = 23,
  R_ETCA_MOV_40 = 24,
  R_ETCA_MOV_45 = 25,
  R_ETCA_MOV_50 = 26,
  R_ETCA_MOV_55 = 27,
  R_ETCA_MOV_60 = 28,
  R_ETCA_MOV_64 = 29,
  R_ETCA_MOV_8 = 30,
  R_ETCA_MOV_16 = 31,
  R_ETCA_MOV_32 = 32,
};

class ETCAELFObjectWriter : public MCELFObjectTargetWriter {
public:
  ETCAELFObjectWriter(uint8_t OSABI, bool Is64Bit)
      : MCELFObjectTargetWriter(Is64Bit, OSABI, ELF::EM_ETCA,
                                /*HasRelocationAddend=*/true) {}

  ~ETCAELFObjectWriter() override = default;

  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    MCFixupKind Kind = Fixup.getKind();

    // Handle generic data fixups (absolute data references).
    if (Kind < FirstTargetFixupKind) {
      switch (unsigned(Kind)) {
      case FK_Data_1:
        return R_ETCA_8;
      case FK_Data_2:
        return R_ETCA_16;
      case FK_Data_4:
        return R_ETCA_32;
      case FK_Data_8:
        return R_ETCA_64;
      default:
        return R_ETCA_NONE;
      }
    }

    // ETCA FixupKind values start at FirstTargetFixupKind.
    // Use a zero-based index into the fixup enum.
    static constexpr unsigned FixupBase = ETCA::fixup_ETCA_NONE;
    unsigned ETCAKind = Kind - FixupBase;

    // Map to relocation types matching the Fixups enum order in
    // ETCAFixupKinds.h.  Keep this in sync with that enum.
    static constexpr unsigned RelocMap[] = {
        R_ETCA_NONE,     // 0: fixup_ETCA_NONE
        R_ETCA_BASE_JMP, // 1: fixup_ETCA_BASE_JMP
        R_ETCA_8,        // 2: fixup_ETCA_8
        R_ETCA_16,       // 3: fixup_ETCA_16
        R_ETCA_32,       // 4: fixup_ETCA_32
        R_ETCA_64,       // 5: fixup_ETCA_64
        R_ETCA_SAF_CALL, // 6: fixup_ETCA_SAF_CALL
        // MOV_* spanning relocs: indices 7-26 → R_ETCA_MOV_5..R_ETCA_MOV_32
        R_ETCA_MOV_5,
        R_ETCA_MOV_10,
        R_ETCA_MOV_15,
        R_ETCA_MOV_20,
        R_ETCA_MOV_25,
        R_ETCA_MOV_30,
        R_ETCA_MOV_35,
        R_ETCA_MOV_40,
        R_ETCA_MOV_45,
        R_ETCA_MOV_50,
        R_ETCA_MOV_55,
        R_ETCA_MOV_60,
        R_ETCA_MOV_64,
        R_ETCA_MOV_8,
        R_ETCA_MOV_16,
        R_ETCA_MOV_32,
    };
    if (ETCAKind < std::size(RelocMap))
      return RelocMap[ETCAKind];
    return R_ETCA_NONE;
  }
};

} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createETCAELFObjectWriter(uint8_t OSABI, bool Is64Bit) {
  return std::make_unique<ETCAELFObjectWriter>(OSABI, Is64Bit);
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

/// Called by the auto-generated emitter for unsupported opcodes.
LLVM_ATTRIBUTE_USED static void reportUnsupportedInst(const MCInst &MI) {
  llvm_unreachable("Unsupported ETCA instruction in emitter");
}

// Auto-generated instruction encoding from TableGen format classes.
#include "ETCAGenMCCodeEmitter.inc"
