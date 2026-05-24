//===- ETCAInstrInfo.td - ETCA Instruction Definitions --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//  Define ETCA instructions
//
//  This file defines all base ISA and SAF extension instructions.
//  Instructions are defined for multiple widths: 16-bit (base), 32-bit (DW),
//  and 64-bit (QW).  The active width is selected via subtarget features.
//
//  Memory LOAD and STORE instructions use 16-bit pointers.  Wider address
//  spaces use dedicated pointer registers.
//===----------------------------------------------------------------------===//

include "ETCAInstrFormats.td"

//===----------------------------------------------------------------------===//
// Predicates for extension availability
//===----------------------------------------------------------------------===//

def HasDW   : Predicate<"Subtarget->hasDW()">;
def HasQW   : Predicate<"Subtarget->hasQW()">;
def HasSAF  : Predicate<"Subtarget->hasSAF()">;
def HasByte : Predicate<"Subtarget->hasByte()">;

//===----------------------------------------------------------------------===//
// Base computation instructions — 16-bit register-register form
//===----------------------------------------------------------------------===//

let isCommutable = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU] in {
  def ADD16   : EInstRR16<0, "add $dst, $src2", []>;
  def SUB16   : EInstRR16<1, "sub $dst, $src2", []>;
  def RSUB16  : EInstRR16<2, "rsub $dst, $src2", []>;
  def OR16    : EInstRR16<4, "or $dst, $src2", []>;
  def XOR16   : EInstRR16<5, "xor $dst, $src2", []>;
  def AND16   : EInstRR16<6, "and $dst, $src2", []>;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU] in {
  // Note: MOVZ/MOVS RR use the non-tied format.  The encoding puts dst in
  // AAA and src in BBB; the (non-existent) tied src1 is a don't-care at the
  // hardware level so we use the simpler 2-operand format.
  def MOVZ16  : EInstRR16_NT<8, "movz $dst, $src", []>;
  def MOVS16  : EInstRR16_NT<9, "movs $dst, $src", []>;
}

// CMP, TEST — dedicated format, width-selectable via SS bits
// Byte (SS=00): requires BYTE extension
// Word (SS=01): base ISA
// Dword (SS=10): requires DW extension
// Qword (SS=11): requires QW extension
let hasSideEffects = 1, Predicates = [HasByte] in {
  def CMP8   : EInstCmp8<3, "cmp $src1, $src2", []>;
  def TEST8  : EInstCmp8<7, "test $src1, $src2", []>;
}
let hasSideEffects = 1 in {
  def CMP    : EInstCmp16<3, "cmp $src1, $src2", []>;
  def TEST   : EInstCmp16<7, "test $src1, $src2", []>;
}
let hasSideEffects = 1, Predicates = [HasDW] in {
  def CMP32  : EInstCmp32<3, "cmp $src1, $src2", []>;
  def TEST32 : EInstCmp32<7, "test $src1, $src2", []>;
}
let hasSideEffects = 1, Predicates = [HasQW] in {
  def CMP64  : EInstCmp64<3, "cmp $src1, $src2", []>;
  def TEST64 : EInstCmp64<7, "test $src1, $src2", []>;
}

//===----------------------------------------------------------------------===//
// LOAD/STORE — RR computation format, width-selectable via SS bits
// CCCC = 1010 (LOAD), 1011 (STORE)
//===----------------------------------------------------------------------===//

// 16-bit load (SS=01)
let mayLoad = 1, hasSideEffects = 0, mayStore = 0 in {
  def LOAD16 : EInstLoadRR16<"load $dst, $addr", []>;
}

// 16-bit store (SS=01)
let mayStore = 1, hasSideEffects = 0, mayLoad = 0 in {
  def STORE16 : EInstStoreRR16<"store $val, $addr", []>;
}

// 32-bit load (SS=10, DW extension)
let mayLoad = 1, hasSideEffects = 0, mayStore = 0, Predicates = [HasDW] in {
  def LOAD32 : EInstLoadRR32<"load $dst, $addr", []>;
}

// 32-bit store (SS=10, DW extension)
let mayStore = 1, hasSideEffects = 0, mayLoad = 0, Predicates = [HasDW] in {
  def STORE32 : EInstStoreRR32<"store $val, $addr", []>;
}

// 64-bit load (SS=11, QW extension)
let mayLoad = 1, hasSideEffects = 0, mayStore = 0, Predicates = [HasQW] in {
  def LOAD64 : EInstLoadRR64<"load $dst, $addr", []>;
}

// 64-bit store (SS=11, QW extension)
let mayStore = 1, hasSideEffects = 0, mayLoad = 0, Predicates = [HasQW] in {
  def STORE64 : EInstStoreRR64<"store $val, $addr", []>;
}

//===----------------------------------------------------------------------===//
// Mixed-width LOAD/STORE — data width differs from pointer width
//
// These have the same encoding as the standard variants (same SS and CCCC)
// but constrain the address register to a different class for the correct
// pointer width.  They are isCodeGenOnly so the auto-generated decoder
// does not see encoding conflicts — the manual disassembler in
// ETCADisassembler.cpp handles all decoding using the standard variants.
//===----------------------------------------------------------------------===//

// 8-bit load with 32-bit pointer (SS=00, BYTE extension)
let mayLoad = 1, hasSideEffects = 0, mayStore = 0, isCodeGenOnly = 1,
    Predicates = [HasByte] in {
  def LOAD8_P32 : EInstLoadRR8P32<"load $dst, $addr", []>;
}

// 8-bit load with 64-bit pointer (SS=00, BYTE extension)
let mayLoad = 1, hasSideEffects = 0, mayStore = 0, isCodeGenOnly = 1,
    Predicates = [HasByte] in {
  def LOAD8_P64 : EInstLoadRR8P64<"load $dst, $addr", []>;
}

// 8-bit store with 32-bit pointer (SS=00, BYTE extension)
let mayStore = 1, hasSideEffects = 0, mayLoad = 0, isCodeGenOnly = 1,
    Predicates = [HasByte] in {
  def STORE8_P32 : EInstStoreRR8P32<"store $val, $addr", []>;
}

// 8-bit store with 64-bit pointer (SS=00, BYTE extension)
let mayStore = 1, hasSideEffects = 0, mayLoad = 0, isCodeGenOnly = 1,
    Predicates = [HasByte] in {
  def STORE8_P64 : EInstStoreRR8P64<"store $val, $addr", []>;
}

// 16-bit load with 32-bit pointer (SS=01)
let mayLoad = 1, hasSideEffects = 0, mayStore = 0, isCodeGenOnly = 1 in {
  def LOAD16_P32 : EInstLoadRR16P32<"load $dst, $addr", []>;
}

// 16-bit load with 64-bit pointer (SS=01)
let mayLoad = 1, hasSideEffects = 0, mayStore = 0, isCodeGenOnly = 1 in {
  def LOAD16_P64 : EInstLoadRR16P64<"load $dst, $addr", []>;
}

// 16-bit store with 32-bit pointer (SS=01)
let mayStore = 1, hasSideEffects = 0, mayLoad = 0, isCodeGenOnly = 1 in {
  def STORE16_P32 : EInstStoreRR16P32<"store $val, $addr", []>;
}

// 16-bit store with 64-bit pointer (SS=01)
let mayStore = 1, hasSideEffects = 0, mayLoad = 0, isCodeGenOnly = 1 in {
  def STORE16_P64 : EInstStoreRR16P64<"store $val, $addr", []>;
}

// 32-bit load with 64-bit pointer (SS=10, DW extension)
let mayLoad = 1, hasSideEffects = 0, mayStore = 0, isCodeGenOnly = 1,
    Predicates = [HasDW] in {
  def LOAD32_P64 : EInstLoadRR32P64<"load $dst, $addr", []>;
}

// 32-bit store with 64-bit pointer (SS=10, DW extension)
let mayStore = 1, hasSideEffects = 0, mayLoad = 0, isCodeGenOnly = 1,
    Predicates = [HasDW] in {
  def STORE32_P64 : EInstStoreRR32P64<"store $val, $addr", []>;
}

//===----------------------------------------------------------------------===//
// Base computation instructions — 16-bit register-immediate form
//===----------------------------------------------------------------------===//

let isCommutable = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU] in {
  def ADDI16  : EInstRI16<0, "add $dst, $imm", []>;
  def SUBI16  : EInstRI16<1, "sub $dst, $imm", []>;
  def RSUBI16 : EInstRI16<2, "rsub $dst, $imm", []>;
  def CMPI16  : EInstCmpRI16<3, "cmp $src1, $imm", []>;
  def ORI16   : EInstRI16<4, "or $dst, $imm", []>;
  def XORI16  : EInstRI16<5, "xor $dst, $imm", []>;
  def ANDI16  : EInstRI16<6, "and $dst, $imm", []>;
  def TESTI16 : EInstCmpRI16<7, "test $src1, $imm", []>;
  // Note: MOVZ/MOVS RI use the non-tied format.  The $dst is encoded in AAA
  // but its value for the $src1 tied-operand is a don't-care at the hardware
  // level, so we use the simpler 2-operand (non-tied) format.
  def MOVZI16 : EInstRI16_NT<8, "movz $dst, $imm", []>;
  def MOVSI16 : EInstRI16_NT<9, "movs $dst, $imm", []>;
}

// SLO — shift left by imm5, OR with low bits
// Uses tied-def format (has $src1 = $dst constraint) since the hardware
// always reads from the destination register.  The TwoAddress pass will
// insert a COPY before SLO when $dst != $src1 in SSA form.  Non-tied
// format cannot be used because it creates SSA violations (same vreg
// defined twice) when chaining MOVZ + SLO for large constant materialization.
let hasSideEffects = 0, mayLoad = 0, mayStore = 0, SchedRW = [WriteALU] in {
  def SLO16   : EInstRI16<12, "slo $dst, $imm", []>;
}

// Control register access
let hasSideEffects = 1, mayLoad = 1, mayStore = 0 in {
  def READCR  : EInstRI16<14, "readcr $dst, $imm", []>;
}
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
  def WRITECR : EInstRI16<15, "writecr $dst, $imm", []>;
}

//===----------------------------------------------------------------------===//
// 8-bit (BYTE extension) instructions — register-register form (SS=00)
//===----------------------------------------------------------------------===//

let isCommutable = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU], Predicates = [HasByte] in {
  def ADD8    : EInstRR8<0, "add $dst, $src2", []>;
  def SUB8    : EInstRR8<1, "sub $dst, $src2", []>;
  def RSUB8   : EInstRR8<2, "rsub $dst, $src2", []>;
  def OR8     : EInstRR8<4, "or $dst, $src2", []>;
  def XOR8    : EInstRR8<5, "xor $dst, $src2", []>;
  def AND8    : EInstRR8<6, "and $dst, $src2", []>;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU], Predicates = [HasByte] in {
  // MOVZ/MOVS RR: zero/sign-extend 8-bit source to register width (SS=00).
  def MOVZ8   : EInstRR8_NT<8, "movz $dst, $src", []>;
  def MOVS8   : EInstRR8_NT<9, "movs $dst, $src", []>;
}

//===----------------------------------------------------------------------===//
// 8-bit (BYTE extension) instructions — register-immediate form (SS=00)
//===----------------------------------------------------------------------===//

let isCommutable = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU], Predicates = [HasByte] in {
  def ADDI8   : EInstRI8<0, "add $dst, $imm", []>;
  def SUBI8   : EInstRI8<1, "sub $dst, $imm", []>;
  def RSUBI8  : EInstRI8<2, "rsub $dst, $imm", []>;
  def CMPI8   : EInstCmpRI8<3, "cmp $src1, $imm", []>;
  def ORI8    : EInstRI8<4, "or $dst, $imm", []>;
  def XORI8   : EInstRI8<5, "xor $dst, $imm", []>;
  def ANDI8   : EInstRI8<6, "and $dst, $imm", []>;
  def TESTI8  : EInstCmpRI8<7, "test $src1, $imm", []>;
  def MOVZI8  : EInstRI8_NT<8, "movz $dst, $imm", []>;
  def MOVSI8  : EInstRI8_NT<9, "movs $dst, $imm", []>;
}

//===----------------------------------------------------------------------===//
// 8-bit (BYTE extension) LOAD/STORE — RR format with SS=00
//===----------------------------------------------------------------------===//

// LOAD8: 8-bit load (SS=00, BYTE extension)
let mayLoad = 1, hasSideEffects = 0, mayStore = 0,
    Predicates = [HasByte] in {
  def LOAD8   : EInstLoadRR8<"load $dst, $addr", []>;
}

// STORE8: 8-bit store (SS=00, BYTE extension)
let mayStore = 1, hasSideEffects = 0, mayLoad = 0,
    Predicates = [HasByte] in {
  def STORE8  : EInstStoreRR8<"store $val, $addr", []>;
}

//===----------------------------------------------------------------------===//
// 32-bit (DW) instructions — register-register form (SS=10)
//===----------------------------------------------------------------------===//

let isCommutable = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU], Predicates = [HasDW] in {
  def ADD32   : EInstRR32<0, "add $dst, $src2", []>;
  def SUB32   : EInstRR32<1, "sub $dst, $src2", []>;
  def RSUB32  : EInstRR32<2, "rsub $dst, $src2", []>;
  def OR32    : EInstRR32<4, "or $dst, $src2", []>;
  def XOR32   : EInstRR32<5, "xor $dst, $src2", []>;
  def AND32   : EInstRR32<6, "and $dst, $src2", []>;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU], Predicates = [HasDW] in {
  def MOVZ32  : EInstRR32_NT<8, "movz $dst, $src", []>;
  def MOVS32  : EInstRR32_NT<9, "movs $dst, $src", []>;
}

//===----------------------------------------------------------------------===//
// 32-bit (DW) instructions — register-immediate form (SS=10)
//===----------------------------------------------------------------------===//

let isCommutable = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU], Predicates = [HasDW] in {
  def ADDI32  : EInstRI32<0, "add $dst, $imm", []>;
  def SUBI32  : EInstRI32<1, "sub $dst, $imm", []>;
  def RSUBI32 : EInstRI32<2, "rsub $dst, $imm", []>;
  def CMPI32  : EInstCmpRI32<3, "cmp $src1, $imm", []>;
  def ORI32   : EInstRI32<4, "or $dst, $imm", []>;
  def XORI32  : EInstRI32<5, "xor $dst, $imm", []>;
  def ANDI32  : EInstRI32<6, "and $dst, $imm", []>;
  def TESTI32 : EInstCmpRI32<7, "test $src1, $imm", []>;
  def MOVZI32 : EInstRI32_NT<8, "movz $dst, $imm", []>;
  def MOVSI32 : EInstRI32_NT<9, "movs $dst, $imm", []>;
}

//===----------------------------------------------------------------------===//
// 64-bit (QW) instructions — register-register form (SS=11)
//===----------------------------------------------------------------------===//

let isCommutable = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU], Predicates = [HasQW] in {
  def ADD64   : EInstRR64<0, "add $dst, $src2", []>;
  def SUB64   : EInstRR64<1, "sub $dst, $src2", []>;
  def RSUB64  : EInstRR64<2, "rsub $dst, $src2", []>;
  def OR64    : EInstRR64<4, "or $dst, $src2", []>;
  def XOR64   : EInstRR64<5, "xor $dst, $src2", []>;
  def AND64   : EInstRR64<6, "and $dst, $src2", []>;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU], Predicates = [HasQW] in {
  def MOVZ64  : EInstRR64_NT<8, "movz $dst, $src", []>;
  def MOVS64  : EInstRR64_NT<9, "movs $dst, $src", []>;
}

//===----------------------------------------------------------------------===//
// 64-bit (QW) instructions — register-immediate form (SS=11)
//===----------------------------------------------------------------------===//

let isCommutable = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU], Predicates = [HasQW] in {
  def ADDI64  : EInstRI64<0, "add $dst, $imm", []>;
  def SUBI64  : EInstRI64<1, "sub $dst, $imm", []>;
  def RSUBI64 : EInstRI64<2, "rsub $dst, $imm", []>;
  def CMPI64  : EInstCmpRI64<3, "cmp $src1, $imm", []>;
  def ORI64   : EInstRI64<4, "or $dst, $imm", []>;
  def XORI64  : EInstRI64<5, "xor $dst, $imm", []>;
  def ANDI64  : EInstRI64<6, "and $dst, $imm", []>;
  def TESTI64 : EInstCmpRI64<7, "test $src1, $imm", []>;
  def MOVZI64 : EInstRI64_NT<8, "movz $dst, $imm", []>;
  def MOVSI64 : EInstRI64_NT<9, "movs $dst, $imm", []>;
}

//===----------------------------------------------------------------------===//
// SAF: Stack instructions — PUSH/POP
//===----------------------------------------------------------------------===//

let Predicates = [HasSAF], hasSideEffects = 1, mayLoad = 1, mayStore = 1 in {
  // POP register — loads from [sp], increments sp (r6)
  def POP   : EInstPOP16<"pop $dst", []>;   // SS=01, 2-byte increment
  def POP32 : EInstPOP<0b10, (outs GPR32:$dst), (ins), "pop $dst", []>;  // SS=10, 4-byte
  def POP64 : EInstPOP<0b11, (outs GPR64:$dst), (ins), "pop $dst", []>;  // SS=11, 8-byte

  // PUSH register — decrements sp (r6), stores to [sp]
  def PUSH  : EInstPUSHR16<"push $src", []>;   // SS=01, 2-byte
  def PUSH32: EInstPUSHR<0b10, (outs), (ins GPR32:$src), "push $src", []>;  // SS=10, 4-byte
  def PUSH64: EInstPUSHR<0b11, (outs), (ins GPR64:$src), "push $src", []>;  // SS=11, 8-byte

  // PUSH immediate
  def PUSHI : EInstPUSHI16<"push $imm", []>;

  // 8-bit PUSH/POP (SS=00, BYTE extension): ±1 byte stack adjustment
  let Predicates = [HasByte] in {
    def POP8   : EInstPOP<0b00, (outs GPR:$dst), (ins), "pop $dst", []>;
    def PUSH8  : EInstPUSHR<0b00, (outs), (ins GPR:$src), "push $src", []>;
    def PUSHI8 : EInstPUSHI<0b00, (outs), (ins i16imm:$imm), "push $imm", []>;
  }
}

//===----------------------------------------------------------------------===//
// Pseudo-instructions
//===----------------------------------------------------------------------===//

// Return from function (SAF: jmp ln)
let hasSideEffects = 1, isPseudo = 1, isCodeGenOnly = 1,
    isReturn = 1, isTerminator = 1, isBarrier = 1 in {
  def RET_Pseudo : EInst<(outs), (ins), "ret", []>;
}

// Function call (SAF: relative or absolute)
// CALL stores return address in R7 and may use argument registers R0-R3.
// Defs = [R7] tells the register allocator that R7 is clobbered.
let hasSideEffects = 1, isPseudo = 1, isCodeGenOnly = 1,
    isCall = 1 in {
  def CALL_Pseudo : EInst<(outs), (ins variable_ops), "call", []> {
    // R7 (link register) is implicitly defined by CALL_Pseudo.
    // Argument registers (R0-R3) are added as implicit uses by lowerCall
    // only for the arguments actually passed, so we do NOT list them here
    // to avoid duplicate implicit operands.
    let Defs = [R7];
  }
}

// Conditional select (pseudo, lowered to cmp + conditional move)
// pred=0: CMP was (or should be) CMP cond, 0; use BNE TrueBB
// pred=1..10: CMP was for ICMP predicate; use matching branch (BEQ, BNE, etc.)
let hasSideEffects = 1, isPseudo = 1, isCodeGenOnly = 1 in {
  def SELECT_Pseudo : EInst<(outs GPR:$dst), (ins GPR:$cond, GPR:$t, GPR:$f, i16imm:$pred),
                             "select $dst, $cond, $t, $f", []>;
}

// Marker pseudo for G_ICMP lowering.  The actual CMP is emitted separately;
// this pseudo carries the predicate so G_BRCOND can find it.
let isPseudo = 1, isCodeGenOnly = 1, hasSideEffects = 0 in {
  def ICMP_Pseudo : EInst<(outs GPR:$dst), (ins GPR:$lhs, GPR:$rhs, i16imm:$pred),
                           "movz $dst, $pred", []>;
}

// Frame setup/destroy pseudo instructions (for SAF prologue/epilogue)
// Mark R6 (SP) as both def and use so the register allocator tracks
// stack pointer modifications across the call sequence.
let hasSideEffects = 1, isPseudo = 1, isCodeGenOnly = 1,
    Defs = [R6], Uses = [R6] in {
  def ADJCALLSTACKDOWN : EInst<(outs), (ins i16imm:$amt1, i16imm:$amt2), "", []>;
  def ADJCALLSTACKUP   : EInst<(outs), (ins i16imm:$amt1, i16imm:$amt2), "", []>;
}

// NOP: encoded as 0x008F (base ISA 2-byte NOP per binutils etca_build_nop:
//   byte0 = 0x8F (10001111), byte1 = 0x00)
let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
  def NOP : EInst<(outs), (ins), "nop", []> {
    let Inst{15-0} = 0x008F;
  }
}

//===----------------------------------------------------------------------===//
// Branch instructions
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1, isBarrier = 1, hasSideEffects = 0,
    SchedRW = [WriteALU] in {
  // Conditional branches: direct flag tests
  //   0010 = BN   (Negative flag N)
  //   0011 = BNN  (Not Negative ~N)
  //   0110 = BOV  (Overflow flag V)
  //   0111 = BNOV (Not Overflow ~V)
  // These are NOT generated by LLVM IR optimization, but must be available
  // for user assembly (binutils compatibility).
  def BN   : EInstBranch<0b0010, "bn $target", []>;
  def BNN  : EInstBranch<0b0011, "bnn $target", []>;
  def BOV  : EInstBranch<0b0110, "bov $target", []>;
  def BNOV : EInstBranch<0b0111, "bnov $target", []>;
  def BR   : EInstBranch<0b1110, "br $target", []> {
    let isBarrier = 1;  // unconditional branch is a barrier
  }
  def BEQ  : EInstBranch<0b0000, "beq $target", []>;
  def BNE  : EInstBranch<0b0001, "bne $target", []>;
  def BLTU : EInstBranch<0b0100, "bltu $target", []>;
  def BGEU : EInstBranch<0b0101, "bgeu $target", []>;
  def BLEU : EInstBranch<0b1000, "bleu $target", []>;
  def BGTU : EInstBranch<0b1001, "bgtu $target", []>;
  def BLT  : EInstBranch<0b1010, "blt $target", []>;
  def BGE  : EInstBranch<0b1011, "bge $target", []>;
  def BLE  : EInstBranch<0b1100, "ble $target", []>;
  def BGT  : EInstBranch<0b1101, "bgt $target", []>;
}

//===----------------------------------------------------------------------===//
// SAF: Call instruction — relative unconditional
//===----------------------------------------------------------------------===//

let Predicates = [HasSAF], isCall = 1, isTerminator = 1, isBarrier = 1,
    hasSideEffects = 1, SchedRW = [WriteALU] in {
  def CALL  : EInstSAFCall<"call $target", []> {
    // R7 (link register) is implicitly defined by CALL (stores return addr).
    // Must match CALL_Pseudo's Defs so the expander can copy operands safely.
    let Defs = [R7];
  }
}

//===----------------------------------------------------------------------===//
// SAF: Absolute register jump/call
//===----------------------------------------------------------------------===//

let Predicates = [HasSAF], isBranch = 1, isTerminator = 1, isBarrier = 1,
    hasSideEffects = 0, SchedRW = [WriteALU] in {
  def JMPR  : EInstSAFJmp<0b1110, GPR, "jmpr $rs", []>;
}

let Predicates = [HasSAF], isCall = 1, isTerminator = 1, isBarrier = 1,
    hasSideEffects = 1, SchedRW = [WriteALU] in {
  def CALLR : EInstSAFCallReg<0b1110, GPR, "callr $rs", []>;
}

//===----------------------------------------------------------------------===//
// Instruction selection patterns
//
// ETCA uses GlobalISel exclusively. All isel logic lives in the
// InstructionSelector (GISel/ETCAInstructionSelector.cpp).  SDAG Pat<>
// patterns (which would require -gen-dag-isel) are deliberately absent.
//
// SLO16 is handled in the InstructionSelector (tied-def, GISel-only).
//===----------------------------------------------------------------------===//
