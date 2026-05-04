//===- ETCAInstrFormats.td - ETCA Instruction Format Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//  Describe ETCA instruction formats
//
//  Each 16-bit instruction has a format marker in the top bits:
//    00xx xxxx xxxx xxxx = 2-register computation (or reserved)
//    01xx xxxx xxxx xxxx = register-immediate computation (or reserved)
//    10xx xxxx xxxx xxxx = (conditional) jump
//    11xx xxxx xxxx xxxx = variable-length, reserved for extensions
//
//  For computation instructions, bits [7:6] (SS) select operand size:
//    00 = 8-bit (BYTE extension)
//    01 = 16-bit (base ISA)
//    10 = 32-bit (DW extension)
//    11 = 64-bit (QW extension)
//===----------------------------------------------------------------------===//

// Operand types for various sized immediates
//
// Note: i16imm, i32imm, i64imm are already defined in Target.td.
//===----------------------------------------------------------------------===//

// 5-bit signed immediate (for i16 RI instructions)
def simm5 : Operand<i16> {
  let DecoderMethod = "decodeSImm5";
}

// 5-bit unsigned immediate (for MOVZ, SLO, LOAD, STORE, READCR, WRITECR)
def uimm5 : Operand<i16> {
  let DecoderMethod = "decodeUImm5";
}

// Branch target operand
def brtarget : Operand<OtherVT> {
  let EncoderMethod = "encodeBranchTarget";
}

// SAF call target (12-bit displacement)
def calltarget : Operand<OtherVT> {
  let EncoderMethod = "encodeCallTarget";
}

//===----------------------------------------------------------------------===//
// Base instruction class: 16-bit fixed width
//===----------------------------------------------------------------------===//

class EInst<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  field bits<16> Inst;
  let Namespace = "ETCA";

  let Size = 2;
  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

//===----------------------------------------------------------------------===//
// Two-register computation format (00 SS CCCC AAA BBB 00)
//
// bits [15:14] = 00 (format marker)
// bits [13:12] = SS (operand size: 01=16b, 10=32b, 11=64b)
// bits [11:8]  = CCCC (opcode)
// bits [7:5]   = AAA (destination + left source)
// bits [4:2]   = BBB (right source)
// bits [1:0]   = 00 (memory mode — reserved for MO1/MO2)
//
// Two-address format: $src1 = $dst, both map to AAA.
//===----------------------------------------------------------------------===//

// Base RR format class with tied-def constraint.
// Used for ALU instructions (ADD, SUB, etc.) where dst == src1.
class EInstRR<bits<2> ss, bits<4> opcod, dag outs, dag ins,
              string asmstr, list<dag> pattern>
    : EInst<outs, ins, asmstr, pattern> {
  bits<3> src1;  // AAA
  bits<3> src2;  // BBB

  let Constraints = "$src1 = $dst";

  let Inst{15-14} = 0b00;
  let Inst{13-12} = ss;     // SS: size bits
  let Inst{11-8}  = opcod;  // CCCC: opcode
  let Inst{7-5}   = src1;   // AAA: destination + left operand
  let Inst{4-2}   = src2;   // BBB: right operand
  let Inst{1-0}   = 0b00;   // MM: memory mode (base=00)
}

// Non-tied RR format class — no constraint.
// Used for MOVZ/MOVS instructions where the src1 value is ignored.
// Only one source operand ($src) since the first source (tied to dst)
// is unused.  The encoder writes dst to the AAA field and src to BBB.
class EInstRR_NT<bits<2> ss, bits<4> opcod, dag outs, dag ins,
                 string asmstr, list<dag> pattern>
    : EInst<outs, ins, asmstr, pattern> {
  bits<3> dst;   // AAA
  bits<3> src;   // BBB

  let Inst{15-14} = 0b00;
  let Inst{13-12} = ss;     // SS: size bits
  let Inst{11-8}  = opcod;  // CCCC: opcode
  let Inst{7-5}   = dst;    // AAA: destination (same as first source in encoding)
  let Inst{4-2}   = src;    // BBB: right operand
  let Inst{1-0}   = 0b00;   // MM: memory mode (base=00)
}

// Convenience: 16-bit RR instruction (SS=01) — tied
class EInstRR16<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRR<0b01, opcod,
              (outs GPR:$dst), (ins GPR:$src1, GPR:$src2),
              asmstr, pattern>;

// Convenience: 32-bit RR instruction (SS=10) — tied
class EInstRR32<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRR<0b10, opcod,
              (outs GPR32:$dst), (ins GPR32:$src1, GPR32:$src2),
              asmstr, pattern>;

// Convenience: 64-bit RR instruction (SS=11) — tied
class EInstRR64<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRR<0b11, opcod,
              (outs GPR64:$dst), (ins GPR64:$src1, GPR64:$src2),
              asmstr, pattern>;

// Convenience: 16-bit RR instruction (SS=01) — non-tied, single source
class EInstRR16_NT<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRR_NT<0b01, opcod,
                 (outs GPR:$dst), (ins GPR:$src),
                 asmstr, pattern>;

// Convenience: 32-bit RR instruction (SS=10) — non-tied, single source
class EInstRR32_NT<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRR_NT<0b10, opcod,
                 (outs GPR32:$dst), (ins GPR32:$src),
                 asmstr, pattern>;

// Convenience: 64-bit RR instruction (SS=11) — non-tied, single source
class EInstRR64_NT<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRR_NT<0b11, opcod,
                 (outs GPR64:$dst), (ins GPR64:$src),
                 asmstr, pattern>;

// Convenience: 8-bit RR instruction (SS=00) — tied (BYTE extension)
class EInstRR8<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRR<0b00, opcod,
              (outs GPR:$dst), (ins GPR:$src1, GPR:$src2),
              asmstr, pattern>;

// Convenience: 8-bit RR instruction (SS=00) — non-tied, single source (BYTE extension)
class EInstRR8_NT<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRR_NT<0b00, opcod,
                 (outs GPR:$dst), (ins GPR:$src),
                 asmstr, pattern>;

//===----------------------------------------------------------------------===//
// Register-immediate computation format (01 SS CCCC AAA IIIII)
//
// bits [15:14] = 01 (format marker)
// bits [13:12] = SS (operand size)
// bits [11:8]  = CCCC (opcode)
// bits [7:5]   = AAA (destination + left source)
// bits [4:0]   = IIIII (5-bit immediate)
//
// For opcodes 0-7: immediate is sign-extended
// For opcodes 8, 10-15: immediate is zero-extended
//===----------------------------------------------------------------------===//

// Tied RI format: for ADD/SUB/etc where dst == src1
class EInstRI<bits<2> ss, bits<4> opcod, dag outs, dag ins,
              string asmstr, list<dag> pattern>
    : EInst<outs, ins, asmstr, pattern> {
  bits<3> src1;  // AAA
  bits<5> imm;   // IIIII
  let Constraints = "$src1 = $dst";

  let Inst{15-14} = 0b01;
  let Inst{13-12} = ss;     // SS: size bits
  let Inst{11-8}  = opcod;  // CCCC: opcode
  let Inst{7-5}   = src1;   // AAA: destination
  let Inst{4-0}   = imm;    // IIIII: 5-bit immediate
}

// Non-tied RI format: for MOVZI/MOVSI/etc where src1 is ignored
class EInstRI_NT<bits<2> ss, bits<4> opcod, dag outs, dag ins,
                 string asmstr, list<dag> pattern>
    : EInst<outs, ins, asmstr, pattern> {
  bits<3> dst;   // AAA
  bits<5> imm;   // IIIII

  let Inst{15-14} = 0b01;
  let Inst{13-12} = ss;     // SS: size bits
  let Inst{11-8}  = opcod;  // CCCC: opcode
  let Inst{7-5}   = dst;    // AAA: destination
  let Inst{4-0}   = imm;    // IIIII: 5-bit immediate
}

// Convenience: 16-bit RI instruction (SS=01) — tied
class EInstRI16<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRI<0b01, opcod,
              (outs GPR:$dst), (ins GPR:$src1, i16imm:$imm),
              asmstr, pattern>;

// Convenience: 32-bit RI instruction (SS=10) — tied
class EInstRI32<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRI<0b10, opcod,
              (outs GPR32:$dst), (ins GPR32:$src1, i32imm:$imm),
              asmstr, pattern>;

// Convenience: 64-bit RI instruction (SS=11) — tied
class EInstRI64<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRI<0b11, opcod,
              (outs GPR64:$dst), (ins GPR64:$src1, i64imm:$imm),
              asmstr, pattern>;

// Convenience: 16-bit RI instruction (SS=01) — non-tied
class EInstRI16_NT<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRI_NT<0b01, opcod,
                 (outs GPR:$dst), (ins i16imm:$imm),
                 asmstr, pattern>;

// Convenience: 32-bit RI instruction (SS=10) — non-tied
class EInstRI32_NT<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRI_NT<0b10, opcod,
                 (outs GPR32:$dst), (ins i32imm:$imm),
                 asmstr, pattern>;

// Convenience: 64-bit RI instruction (SS=11) — non-tied
class EInstRI64_NT<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRI_NT<0b11, opcod,
                 (outs GPR64:$dst), (ins i64imm:$imm),
                 asmstr, pattern>;

// Convenience: 8-bit RI instruction (SS=00) — tied (BYTE extension)
class EInstRI8<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRI<0b00, opcod,
              (outs GPR:$dst), (ins GPR:$src1, i16imm:$imm),
              asmstr, pattern>;

// Convenience: 8-bit RI instruction (SS=00) — non-tied (BYTE extension)
class EInstRI8_NT<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstRI_NT<0b00, opcod,
                 (outs GPR:$dst), (ins i16imm:$imm),
                 asmstr, pattern>;

//===----------------------------------------------------------------------===//
// Branch instruction format (10 0 D CCCC DDDDDDDD)
//
// bits [15:14] = 10 (format marker)
// bit  [13]   = 0  (branch indicator)
// bit  [12]   = D8 (high bit of 9-bit signed displacement)
// bits [11:8]  = CCCC (condition code)
// bits [7:0]   = D[7:0] (low 8 bits of displacement)
//
// Displacement is sign-extended from 9 bits, added to current PC.
//===----------------------------------------------------------------------===//

class EInstBranch<bits<4> cond_val, string asmstr, list<dag> pattern>
    : EInst<(outs), (ins brtarget:$target), asmstr, pattern> {
  bits<9> target;

  let Inst{15-14} = 0b10;
  let Inst{13}    = 0;
  let Inst{12}    = target{8};
  let Inst{11-8}  = cond_val;
  let Inst{7-0}   = target{7-0};

  let mayLoad = 0;
  let mayStore = 0;
  let hasSideEffects = 0;
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 0;  // conditional branches fall through
}

//===----------------------------------------------------------------------===//
// SAF: Unconditional relative call (10 1 1 DDDD DDDDDDDD)
//
// bits [15:14] = 10
// bits [13:12] = 11 (SAF call indicator)
// bits [11:0]  = 12-bit signed displacement
//===----------------------------------------------------------------------===//

class EInstSAFCall<string asmstr, list<dag> pattern>
    : EInst<(outs), (ins calltarget:$target), asmstr, pattern> {
  bits<12> target;

  let Inst{15-12} = 0b1011;
  let Inst{11-0}  = target;

  let mayLoad = 0;
  let mayStore = 0;
  let hasSideEffects = 1;
  let isCall = 1;
  let isTerminator = 1;
  let isBarrier = 1;
}

//===----------------------------------------------------------------------===//
// SAF: Absolute register jump/call (10 1 0 1111 RRR X CCCC)
//
// bits [15:13] = 101
// bit  [12]   = 0
// bits [11:8]  = 1111 (SAF indicator)
// bits [7:5]   = RRR (target register)
// bit  [4]    = X (0=jump, 1=call)
// bits [3:0]   = CCCC (condition code)
//===----------------------------------------------------------------------===//

// Conditional absolute register jump
class EInstSAFJmp<bits<4> cond_val, string asmstr, list<dag> pattern>
    : EInst<(outs), (ins GPR:$rs), asmstr, pattern> {
  bits<3> rs;
  let Inst{15-13} = 0b101;
  let Inst{12}    = 0;
  let Inst{11-8}  = 0b1111;
  let Inst{7-5}   = rs;
  let Inst{4}     = 0;
  let Inst{3-0}   = cond_val;

  let mayLoad = 0;
  let mayStore = 0;
  let hasSideEffects = 0;
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
}

// Conditional absolute register call
class EInstSAFCallReg<bits<4> cond_val, string asmstr, list<dag> pattern>
    : EInst<(outs), (ins GPR:$rs), asmstr, pattern> {
  bits<3> rs;
  let Inst{15-13} = 0b101;
  let Inst{12}    = 0;
  let Inst{11-8}  = 0b1111;
  let Inst{7-5}   = rs;
  let Inst{4}     = 1;
  let Inst{3-0}   = cond_val;

  let mayLoad = 0;
  let mayStore = 0;
  let hasSideEffects = 1;
  let isCall = 1;
  let isTerminator = 1;
  let isBarrier = 1;
}

//===----------------------------------------------------------------------===//
// SAF: Stack instructions (PUSH/POP)
//
// PUSH reg:  00 SS 1101 110 RRR 00
// POP  reg:  00 SS 1100 RRR 110 00
//
// bits [15:14] = 00
// bits [13:12] = SS (determines stack increment size)
// bits [11:8]  = 1100 (POP) or 1101 (PUSH)
// bits [7:5]   = RRR (register)
// bits [4:2]   = 110
// bits [1:0]   = 00
//
// PUSH imm: 01 SS 1101 110 IIIII
//===----------------------------------------------------------------------===//

// POP register instruction (00 SS 1100 RRR 110 00)
class EInstPOP<bits<2> ss, dag outs, dag ins,
               string asmstr, list<dag> pattern>
    : EInst<outs, ins, asmstr, pattern> {
  bits<3> dst;

  let Inst{15-14} = 0b00;
  let Inst{13-12} = ss;
  let Inst{11-8}  = 0b1100;
  let Inst{7-5}   = dst;
  let Inst{4-2}   = 0b110;
  let Inst{1-0}   = 0b00;

  let mayLoad = 1;
  let mayStore = 1;
  let hasSideEffects = 1;
}

// POP 16-bit
class EInstPOP16<string asmstr, list<dag> pattern>
    : EInstPOP<0b01, (outs GPR:$dst), (ins),
               asmstr, pattern>;

// PUSH register instruction (00 SS 1101 110 RRR 00)
class EInstPUSHR<bits<2> ss, dag outs, dag ins,
                 string asmstr, list<dag> pattern>
    : EInst<outs, ins, asmstr, pattern> {
  bits<3> src;

  let Inst{15-14} = 0b00;
  let Inst{13-12} = ss;
  let Inst{11-8}  = 0b1101;
  let Inst{7-5}   = 0b110;
  let Inst{4-2}   = src;
  let Inst{1-0}   = 0b00;

  let mayLoad = 1;
  let mayStore = 1;
  let hasSideEffects = 1;
}

// PUSH register 16-bit
class EInstPUSHR16<string asmstr, list<dag> pattern>
    : EInstPUSHR<0b01, (outs), (ins GPR:$src),
                 asmstr, pattern>;

// PUSH immediate instruction (01 SS 1101 110 IIIII)
class EInstPUSHI<bits<2> ss, dag outs, dag ins,
                 string asmstr, list<dag> pattern>
    : EInst<outs, ins, asmstr, pattern> {
  bits<5> imm;

  let Inst{15-14} = 0b01;
  let Inst{13-12} = ss;
  let Inst{11-8}  = 0b1101;
  let Inst{7-5}   = 0b110;
  let Inst{4-0}   = imm;

  let mayLoad = 1;
  let mayStore = 1;
  let hasSideEffects = 1;
}

// PUSH immediate 16-bit
class EInstPUSHI16<string asmstr, list<dag> pattern>
    : EInstPUSHI<0b01, (outs), (ins i16imm:$imm),
                 asmstr, pattern>;

//===----------------------------------------------------------------------===//
// LOAD/STORE using RR computation format (00 SS CCCC AAA BBB MM)
//
// These use CCCC=1010 for LOAD, CCCC=1011 for STORE.
// The SS bits determine operand width:
//   00 = 8-bit (BYTE extension)
//   01 = 16-bit (base ISA)
//   10 = 32-bit (DW extension)
//   11 = 64-bit (QW extension)
//
// This replaces the older dedicated-format LOAD/STORE (bits 000100 + opcode)
// which only supported 16-bit operations.  The RR format supports all widths.
//===----------------------------------------------------------------------===//

// Generic RR-format load: loads from MEM[addr] into dst
class EInstLoadRR<bits<2> ss, dag outs, string asmstr, list<dag> pattern>
    : EInst<outs, (ins GPR:$addr), asmstr, pattern> {
  bits<3> dst;
  bits<3> addr;
  let Inst{15-14} = 0b00;    // format = computation
  let Inst{13-12} = ss;      // SS: operand size
  let Inst{11-8}  = 0b1010;  // CCCC = LOAD opcode
  let Inst{7-5}   = dst;     // AAA: destination
  let Inst{4-2}   = addr;    // BBB: address
  let Inst{1-0}   = 0b00;    // MM: base
  let mayLoad = 1;
  let hasSideEffects = 0;
  let SchedRW = [WriteLD];
}

// Convenience: 8-bit load (SS=00)
class EInstLoadRR8<string asmstr, list<dag> pattern>
    : EInstLoadRR<0b00, (outs GPR:$dst), asmstr, pattern>;

// Convenience: 16-bit load (SS=01)
class EInstLoadRR16<string asmstr, list<dag> pattern>
    : EInstLoadRR<0b01, (outs GPR:$dst), asmstr, pattern>;

// Convenience: 32-bit load (SS=10)
class EInstLoadRR32<string asmstr, list<dag> pattern>
    : EInstLoadRR<0b10, (outs GPR32:$dst), asmstr, pattern>;

// Convenience: 64-bit load (SS=11)
class EInstLoadRR64<string asmstr, list<dag> pattern>
    : EInstLoadRR<0b11, (outs GPR64:$dst), asmstr, pattern>;

// Generic RR-format store base (no address in ins list yet)
class EInstStoreRRBase<bits<2> ss, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EInst<outs, ins, asmstr, pattern> {
  bits<3> val;
  bits<3> addr;
  let Inst{15-14} = 0b00;    // format = computation
  let Inst{13-12} = ss;      // SS: operand size
  let Inst{11-8}  = 0b1011;  // CCCC = STORE opcode
  let Inst{7-5}   = val;     // AAA: value to store
  let Inst{4-2}   = addr;    // BBB: address
  let Inst{1-0}   = 0b00;    // MM: base
  let mayStore = 1;
  let hasSideEffects = 0;
  let SchedRW = [WriteST];
}

// 8-bit store (SS=00)
class EInstStoreRR8<string asmstr, list<dag> pattern>
    : EInstStoreRRBase<0b00, (outs), (ins GPR:$val, GPR:$addr), asmstr, pattern>;

// 16-bit store (SS=01)
class EInstStoreRR16<string asmstr, list<dag> pattern>
    : EInstStoreRRBase<0b01, (outs), (ins GPR:$val, GPR:$addr), asmstr, pattern>;

// 32-bit store (SS=10)
class EInstStoreRR32<string asmstr, list<dag> pattern>
    : EInstStoreRRBase<0b10, (outs), (ins GPR32:$val, GPR:$addr), asmstr, pattern>;

// 64-bit store (SS=11)
class EInstStoreRR64<string asmstr, list<dag> pattern>
    : EInstStoreRRBase<0b11, (outs), (ins GPR64:$val, GPR:$addr), asmstr, pattern>;

// Compare/test instruction format — takes SS parameter for width.
// SS=01 (word, base), SS=10 (dword, DW), SS=11 (qword, QW)
// Byte0 bits [7:6]=00 (RR format), [5:4]=SS (size), [3:0]=CCCC (opcode)
class EInstCmp<bits<2> ss, bits<4> opcod, string asmstr, list<dag> pattern>
    : EInst<(outs), (ins GPR:$src1, GPR:$src2), asmstr, pattern> {
  bits<3> src1;
  bits<3> src2;
  let Inst{15-13} = src1;
  let Inst{12-10} = src2;
  let Inst{9-8}   = 0b00;    // MM (memory mode, base=00)
  let Inst{7-6}   = 0b00;    // bits 7-6: RR format marker (00 for 2-reg computation)
  let Inst{5-4}   = ss;      // SS: size bits (01=word, 10=dword, 11=qword)
  let Inst{3-0}   = opcod;   // CCCC: opcode
  let hasSideEffects = 0;
}

// Convenience: byte CMP/TEST (SS=00)
class EInstCmp8<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstCmp<0b00, opcod, asmstr, pattern>;

// Convenience: word CMP/TEST (SS=01)
class EInstCmp16<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstCmp<0b01, opcod, asmstr, pattern>;

// Convenience: dword CMP/TEST (SS=10)
class EInstCmp32<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstCmp<0b10, opcod, asmstr, pattern>;

// Convenience: qword CMP/TEST (SS=11)
class EInstCmp64<bits<4> opcod, string asmstr, list<dag> pattern>
    : EInstCmp<0b11, opcod, asmstr, pattern>;
