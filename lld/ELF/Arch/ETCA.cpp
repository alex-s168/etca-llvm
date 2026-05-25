//===- ETCA.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The ETCA is a 16-bit RISC architecture with a variable-length instruction
// set. It is a bare-metal target designed for embedded systems. All memory
// is directly addressable with no MMU. The architecture supports multiple
// word/address size combinations: 16b+16b, 32b+32b, 32b+64b, 64b+32b, 64b+64b.
//
// This file implements link-time relocation processing for ELF objects
// targeting the ETCA architecture, matching the binutils implementation
// in elf32-etca.c.
//
// All relocations use RELA format (explicit addends, partial_inplace=false).
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {

// ETCA relocation type constants matching binutils include/elf/etca.h
enum {
  R_ETCA_NONE = 0,
  R_ETCA_BASE_JMP = 1,
  R_ETCA_EXABS_8 = 2,
  R_ETCA_EXABS_16 = 3,
  R_ETCA_EXABS_32 = 4,
  R_ETCA_EXABS_64 = 5,
  R_ETCA_SAF_CALL = 6,
  R_ETCA_ABM_RIS_5 = 7,
  R_ETCA_ABM_RIZ_5 = 8,
  R_ETCA_ABM_RIS_8 = 9,
  R_ETCA_ABM_RIZ_8 = 10,
  R_ETCA_ABM_RIS_16 = 11,
  R_ETCA_ABM_RIZ_16 = 12,
  R_ETCA_ABM_RIS_32 = 13,
  R_ETCA_ABM_RIZ_32 = 14,
  R_ETCA_ABM_RIS_64 = 15,
  R_ETCA_ABM_RIZ_64 = 16,
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
  R_ETCA_MOV_5_REX = 33,
  R_ETCA_MOV_10_REX = 34,
  R_ETCA_MOV_15_REX = 35,
  R_ETCA_MOV_20_REX = 36,
  R_ETCA_MOV_25_REX = 37,
  R_ETCA_MOV_30_REX = 38,
  R_ETCA_MOV_35_REX = 39,
  R_ETCA_MOV_40_REX = 40,
  R_ETCA_MOV_45_REX = 41,
  R_ETCA_MOV_50_REX = 42,
  R_ETCA_MOV_55_REX = 43,
  R_ETCA_MOV_60_REX = 44,
  R_ETCA_MOV_64_REX = 45,
  R_ETCA_MOV_8_REX = 46,
  R_ETCA_MOV_16_REX = 47,
  R_ETCA_MOV_32_REX = 48,
  R_ETCA_8 = 49,
  R_ETCA_16 = 50,
  R_ETCA_32 = 51,
  R_ETCA_64 = 52,
  R_ETCA_IPREL_8 = 53,
  R_ETCA_IPREL_16 = 54,
  R_ETCA_IPREL_32 = 55,
  R_ETCA_IPREL_64 = 56,
};

// Helper macros matching binutils include/elf/etca.h
#define R_ETCA_IS_EXABS(r_type)                                               \
  (R_ETCA_EXABS_8 <= (r_type) && (r_type) <= R_ETCA_EXABS_64)
#define R_ETCA_IS_DISP(r_type)                                                \
  (R_ETCA_8 <= (r_type) && (r_type) <= R_ETCA_64)
#define R_ETCA_IS_IPREL(r_type)                                               \
  (R_ETCA_IPREL_8 <= (r_type) && (r_type) <= R_ETCA_IPREL_64)
#define R_ETCA_IS_ANY_DISP(r_type)                                            \
  (R_ETCA_IS_EXABS(r_type) || R_ETCA_IS_DISP(r_type) ||                      \
   R_ETCA_IS_IPREL(r_type))
#define R_ETCA_IS_ABM_IMM(r_type)                                             \
  (R_ETCA_ABM_RIS_5 <= (r_type) && (r_type) <= R_ETCA_ABM_RIZ_64)
#define R_ETCA_IS_MOV(r_type)                                                 \
  (R_ETCA_MOV_5 <= (r_type) && (r_type) <= R_ETCA_MOV_32)
#define R_ETCA_IS_MOV_REX(r_type)                                             \
  (R_ETCA_MOV_5_REX <= (r_type) && (r_type) <= R_ETCA_MOV_32_REX)

// Compute number of instructions for a MOV relocation type.
// Matches R_ETCA_MOV_TO_INSTRUCTION_COUNT from binutils.
static unsigned movRelocToInsnCount(unsigned type) {
  unsigned norm = type;
  if (R_ETCA_IS_MOV_REX(type))
    norm = type - (R_ETCA_MOV_5_REX - R_ETCA_MOV_5);

  if (norm == R_ETCA_MOV_8)
    return 2;
  if (norm == R_ETCA_MOV_16)
    return 4;
  if (norm == R_ETCA_MOV_32)
    return 7;
  // For MOV_5 through MOV_64: insn count = N - MOV_5 + 1
  return norm - R_ETCA_MOV_5 + 1;
}

// Get the size (SS) field from the MOV instruction's first opcode byte.
// SS = bits[5:4] of the first instruction byte.
// For REX sequences, the first instruction byte is at offset 1 (after REX).
static int8_t getMovSize(const uint8_t *loc, unsigned type) {
  const uint8_t *firstInsn = loc;
  if (R_ETCA_IS_MOV_REX(type))
    firstInsn += 1; // skip REX prefix byte
  return (firstInsn[0] >> 4) & 3;
}

// Get the register number from a MOV instruction's ARG byte.
// Register field = bits[7:5] of the ARG byte.
// For REX sequences, the ARG byte is after the REX prefix + opcode.
static unsigned getMovReg(const uint8_t *loc, unsigned type) {
  unsigned reg;
  if (R_ETCA_IS_MOV_REX(type)) {
    // REX prefix at loc[0], opcode at loc[1], ARG at loc[2]
    reg = (loc[2] >> 5) & 7;
    reg |= 0b1000; // REX extends register number
  } else {
    // Opcode at loc[0], ARG at loc[1]
    reg = (loc[1] >> 5) & 7;
  }
  return reg;
}

// Sign-extend a value to the given bit width.
static int64_t signExtend(uint64_t val, unsigned bits) {
  if (bits >= 64)
    return val;
  uint64_t mask = (1ULL << bits) - 1;
  val &= mask;
  // If the sign bit is set, extend with ones.
  if (val & (1ULL << (bits - 1)))
    val |= ~mask;
  return (int64_t)val;
}

// Write a 5-bit section of a value into an instruction byte.
// Format: [reg(7:5)][imm5(4:0)]
static void write5BitSection(uint8_t *loc, unsigned reg, uint64_t value,
                              unsigned section) {
  unsigned fiveBitVal = (value >> (section * 5)) & 0x1F;
  *loc = (reg << 5) | fiveBitVal;
}

// Build a MOV RI sequence at the given location, overwriting the existing
// placeholder. Matches etca_build_mov_ri() from binutils.
static void buildMovRi(uint8_t *loc, int64_t value, unsigned relocType) {
  const bool needRex = R_ETCA_IS_MOV_REX(relocType);
  const unsigned insnCount = movRelocToInsnCount(relocType);
  const int8_t size = getMovSize(loc, relocType);
  const unsigned reg = getMovReg(loc, relocType);
  const unsigned bitWidth = (size == 0) ? 8 : (size == 1) ? 16 : (size == 2) ? 32 : 64;

  // Sign-extend the value to the operation width.
  int64_t sextValue = signExtend(value, bitWidth);
  uint64_t uvalue = (uint64_t)sextValue;

  // Determine the actual number of 5-bit sections needed.
  // Count the number of 5-bit chunks (rounding up).
  unsigned neededSections = (bitWidth + 4) / 5;
  if (neededSections > insnCount)
    neededSections = insnCount;

  // Check if the top section is all zeros or all ones (can be elided).
  // (Matching binutils logic: if the top 5-bit chunk is 0 or 0x1F, reduce
  //  the section count by 1.)
  if (neededSections > 1) {
    unsigned topSection = neededSections - 1;
    uint64_t topVal = (uvalue >> (topSection * 5)) & 0x1F;
    if (topVal == 0 || topVal == 0x1F)
      neededSections--;
  }
  if (neededSections < 1)
    neededSections = 1;

  // NOP padding: if the actual needed sections is fewer than the relocation
  // type encodes, fill the rest with NOPs (0x008F per instruction).
  unsigned padInsns = (neededSections < insnCount) ? (insnCount - neededSections) : 0;

  // Encode the initial MOV instruction.
  size_t idx = 0;
  if (needRex)
    loc[idx++] = 0b11000100; // REX prefix

  // MOVZ or MOVS opcode: 0b01001000|(size<<4) or 0b01001001|(size<<4)
  // Check if the sign bit of the value is set.
  uint64_t signBit = 1ULL << (bitWidth - 1);
  if (uvalue & signBit) {
    // Sign-extending: use MOVS
    loc[idx++] = 0b01001001 | (size << 4);
  } else {
    // Zero-extending: use MOVZ
    loc[idx++] = 0b01001000 | (size << 4);
  }
  write5BitSection(&loc[idx++], reg, uvalue, (int)neededSections - 1);

  // Encode SLO (Shift Left Or) instructions for remaining sections.
  for (int i = (int)neededSections - 2; i >= 0; i--) {
    if (needRex)
      loc[idx++] = 0b11000100;
    loc[idx++] = 0b01001100 | (size << 4); // SLO opcode
    write5BitSection(&loc[idx++], reg, uvalue, i);
  }

  // Pad with NOPs (0x008F per instruction = [0x8F, 0x00] LE).
  for (unsigned i = 0; i < padInsns; i++) {
    loc[idx++] = 0x8F;
    loc[idx++] = 0x00;
  }
}

class ETCA final : public TargetInfo {
public:
  ETCA(Ctx &ctx) : TargetInfo(ctx) {
    // For bare-metal ETCA, use the minimal page size (no virtual memory).
    defaultCommonPageSize = 1;
    defaultMaxPageSize = 1;

    // Trap instruction: 0x008F (canonical NOP per binutils etca_build_nop).
    trapInstr = {0x8F, 0x00, 0x8F, 0x00};

    // Default image base for bare-metal.
    defaultImageBase = 0x0000;
  }

  uint32_t calcEFlags() const override;
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
  bool usesOnlyLowPageBits(RelType type) const override;
};

} // namespace

void elf::setETCATargetInfo(Ctx &ctx) { ctx.target.reset(new ETCA(ctx)); }

uint32_t ETCA::calcEFlags() const {
  // ETCA doesn't use ELF flags currently. Return 0.
  return 0;
}

RelExpr ETCA::getRelExpr(RelType type, const Symbol &s,
                         const uint8_t *loc) const {
  switch (type) {
  case R_ETCA_BASE_JMP:
  case R_ETCA_SAF_CALL:
  case R_ETCA_IPREL_8:
  case R_ETCA_IPREL_16:
  case R_ETCA_IPREL_32:
  case R_ETCA_IPREL_64:
    return R_PC;
  case R_ETCA_NONE:
    return R_NONE;
  default:
    // All other relocations are absolute:
    //   R_ETCA_EXABS_*, R_ETCA_ABM_RIS_*, R_ETCA_ABM_RIZ_*,
    //   R_ETCA_MOV_*, R_ETCA_MOV_*_REX,
    //   R_ETCA_8, R_ETCA_16, R_ETCA_32, R_ETCA_64
    return R_ABS;
  }
}

void ETCA::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  RelType type = rel.type;

  // Compute the relocated value.
  // For PC-relative relocations, val already includes the PC adjustment
  // (it's S + A - P). For absolute relocations, val is S + A.
  // The binutils perform_relocation does: value -= P for PC-relative,
  // then adds the addend, so our val is already correct.

  switch (type) {
  case R_ETCA_NONE:
    break;

  case R_ETCA_BASE_JMP: {
    // 9-bit signed PC-relative branch displacement.
    // Value is S + A - P.
    // Encoding:
    //   bit 8 → byte 0 bit 4
    //   bits 7:0 → byte 1
    checkInt(ctx, loc, val, 9, rel);
    loc[0] |= (val & 0x100) ? 0x10 : 0;
    loc[1] = val & 0xFF;
    break;
  }

  case R_ETCA_SAF_CALL: {
    // 12-bit signed PC-relative call displacement.
    // Value is S + A - P.
    // Encoding:
    //   bits 11:8 → byte 0 bits 3:0
    //   bits 7:0 → byte 1
    checkInt(ctx, loc, val, 12, rel);
    loc[0] |= (val >> 8) & 0x0F;
    loc[1] = val & 0xFF;
    break;
  }

  case R_ETCA_EXABS_8:
  case R_ETCA_ABM_RIS_8:
  case R_ETCA_ABM_RIZ_8:
  case R_ETCA_IPREL_8:
  case R_ETCA_8: {
    // 8-bit value (absolute or PC-relative after adjustment).
    checkIntUInt(ctx, loc, val, 8, rel);
    *loc = val & 0xFF;
    break;
  }

  case R_ETCA_EXABS_16:
  case R_ETCA_ABM_RIS_16:
  case R_ETCA_ABM_RIZ_16:
  case R_ETCA_IPREL_16:
  case R_ETCA_16: {
    // 16-bit value.
    checkIntUInt(ctx, loc, val, 16, rel);
    write16le(loc, val & 0xFFFF);
    break;
  }

  case R_ETCA_EXABS_32:
  case R_ETCA_ABM_RIS_32:
  case R_ETCA_ABM_RIZ_32:
  case R_ETCA_IPREL_32:
  case R_ETCA_32: {
    // 32-bit value.
    checkIntUInt(ctx, loc, val, 32, rel);
    write32le(loc, val);
    break;
  }

  case R_ETCA_EXABS_64:
  case R_ETCA_ABM_RIS_64:
  case R_ETCA_ABM_RIZ_64:
  case R_ETCA_IPREL_64:
  case R_ETCA_64: {
    // 64-bit value.
    checkIntUInt(ctx, loc, val, 64, rel);
    write64le(loc, val);
    break;
  }

  case R_ETCA_ABM_RIS_5: {
    // 5-bit signed immediate (ABM instruction format).
    checkInt(ctx, loc, val, 5, rel);
    loc[0] = (loc[0] & 0xE0) | (val & 0x1F);
    break;
  }

  case R_ETCA_ABM_RIZ_5: {
    // 5-bit unsigned immediate (ABM instruction format).
    checkUInt(ctx, loc, val, 5, rel);
    loc[0] = (loc[0] & 0xE0) | (val & 0x1F);
    break;
  }

  default:
    if (R_ETCA_IS_MOV(type) || R_ETCA_IS_MOV_REX(type)) {
      // MOV RI sequence: re-encode the entire multi-instruction sequence.
      buildMovRi(loc, val, type);
      break;
    }
    Err(ctx) << getErrorLoc(ctx, loc)
             << "unrecognized relocation " << rel.type;
    break;
  }
}

bool ETCA::usesOnlyLowPageBits(RelType type) const {
  // ETCA doesn't use page-relative relocations (no ADRP-like instructions
  // that need linker relaxation of page-offset pairs), so no relocation
  // uses only low page bits.
  return false;
}
