//=- ETCARegisterInfo.td - ETCA Register defs ---------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//  Declarations that describe the ETCA register file
//
//  ETCA supports variable register widths depending on enabled extensions:
//  - Base ISA: 16-bit registers (word)
//  - +DW (CP1.14): 32-bit registers (doubleword) with 16-bit sub-regs
//  - +QW (CP1.15): 64-bit registers (quadword) with 32/16-bit sub-regs
//
//  Physical registers are always named r0-r7 regardless of width.
//  Sub-register indices provide access to narrower views.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Sub-register indices for multi-width register access.
//===----------------------------------------------------------------------===//

// For 32-bit registers (DW extension): sub_16 gives low 16 bits
def sub_16 : SubRegIndex<16>;

// For 64-bit registers (QW extension): sub_32 gives low 32 bits
def sub_32 : SubRegIndex<32>;
// sub_16_hi gives bits [31:16] for 64-bit registers
def sub_16_hi : SubRegIndex<16, 16>;

// For 8-bit values (BYTE extension): sub_8 gives low 8 bits of a 16-bit register
def sub_8 : SubRegIndex<8>;

// 64-bit register has:
//   sub_32 → low 32 bits (d0-d7)
//   sub_16 → low 16 bits (r0-r7) mapped through sub_32
//   sub_16_hi → bits [31:16]
// 32-bit register has:
//   sub_16 → low 16 bits (r0-r7)

//===----------------------------------------------------------------------===//
// Base 16-bit registers — always present
//===----------------------------------------------------------------------===//

class ETCAReg<bits<4> num, string n, list<Register> subregs = [],
               list<string> altNames = []> : Register<n, altNames> {
  field bits<4> Num;
  let Num = num;
  let HWEncoding{3-0} = num;
  let Namespace = "ETCA";
  let SubRegs = subregs;
}

// 16-bit word registers (base ISA: r0-r7, REX: r8-r15)
foreach i = 0-15 in {
  def R#i : ETCAReg<i, "r"#i>, DwarfRegNum<[i]>;
}

// 32-bit doubleword registers (DW extension)
// Binutils-compatible name: r0d, r1d, ..., r15d
// All D registers share the same physical storage as the corresponding R
// register (sub_16 → R), including REX extended registers (r8d has
// sub_16 → r8).
foreach i = 0-15 in {
  def D#i : ETCAReg<i, "r"#i#"d", [!cast<Register>("R"#i)], ["d"#i, "r"#i]>,
             DwarfRegNum<[!add(i, 16)]> {
    // Connect sub_16 from D reg to R reg
    let SubRegIndices = [sub_16];
  }
}

// 64-bit quadword registers (QW extension)
// Binutils-compatible name: r0q, r1q, ..., r15q
// All Q registers share the same physical storage as the corresponding D
// register (sub_32 → D, transitively sub_16 → R via D), including REX
// extended registers.
foreach i = 0-15 in {
  def Q#i : ETCAReg<i, "r"#i#"q",
                     [!cast<Register>("D"#i)],
                     ["q"#i, "d"#i, "r"#i]>,
             DwarfRegNum<[!add(i, 32)]> {
    // Connect sub_32 from Q reg to D reg.
    // R regs are reachable via sub_32_then_sub_16 (D already has sub_16 -> R).
    let SubRegIndices = [sub_32];
    let CoveredBySubRegs = 1;
  }
}

//===----------------------------------------------------------------------===//
// Register classes for each width.
// Only the classes matching the target word size are allocatable.
//===----------------------------------------------------------------------===//

// 16-bit GPR class (base 8 regs for codegen)
def GPR : RegisterClass<"ETCA", [i16], 16,
    (add R0, R1, R2, R3, R4, R5, R6, R7)> {
  let Size = 16;
}

// 32-bit GPR class (base 8 regs for codegen)
def GPR32 : RegisterClass<"ETCA", [i32], 32,
    (add D0, D1, D2, D3, D4, D5, D6, D7)> {
  let Size = 32;
}

// 64-bit GPR class (base 8 regs for codegen)
def GPR64 : RegisterClass<"ETCA", [i64], 64,
    (add Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7)> {
  let Size = 64;
}

// 8-bit GPR class (BYTE extension)
// Uses the same physical registers (R0-R7) as the 16-bit GPR class.
// Size is explicit for i8 type.
def GPR8 : RegisterClass<"ETCA", [i8], 8,
    (add R0, R1, R2, R3, R4, R5, R6, R7)> {
  let Size = 8;
}

// 8-bit GPR class with REX extension (BYTE extension + REX)
// Provides 16 registers (R0-R15) for 8-bit values on REX-enabled targets.
// Currently not wired into the default register allocator (use GPR8 for that);
// defined here for completeness so calling convention and CSR masks can
// reference it when needed.
def GPR8_REX : RegisterClass<"ETCA", [i8], 8,
    (add R0, R1, R2, R3, R4, R5, R6, R7,
         R8, R9, R10, R11, R12, R13, R14, R15)> {
  let Size = 8;
}


