//===- EtcaRegisterInfo.td - Etca Register defs ------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//  Declarations that describe the Etca register file
//===----------------------------------------------------------------------===//

// Registers are identified with 5 bit IDS:
// 0-15 ETC.A core and extended registers 
// 16   virtual flags register (for cg stuff)
class EtcaReg<bits<5> num, string n, list<Register> subregs = [],
              list<string> altNames = []> : Register<n, altNames> {
  field bits<5> Num;
  let Num = num;
  let Namespace = "Etca";
  let SubRegs = subregs;
}

let Namespace = "Etca" in {
  def sub_32 : SubRegIndex<32>;
}

// Integer registers
foreach i = 0-7 in {
  def R#i : EtcaReg<i, "r"#i>, DwarfRegNum<[i]>;
}

// Register aliases
let SubRegIndices = [sub_32] in {
  def A0 : EtcaReg<0,  "a0", [R0],  ["v0"]>, DwarfRegAlias<R0 >;
  def A1 : EtcaReg<1,  "a1", [R1],  ["v1"]>, DwarfRegAlias<R1 >;
  def A2 : EtcaReg<2,  "a2", [R2]         >, DwarfRegAlias<R2 >;
  def S0 : EtcaReg<3,  "s0", [R3]         >, DwarfRegAlias<R3 >;
  def S1 : EtcaReg<4,  "s1", [R4]         >, DwarfRegAlias<R4 >;
  def BP : EtcaReg<5,  "bp", [R5]         >, DwarfRegAlias<R5 >;
  def SP : EtcaReg<6,  "sp", [R6]         >, DwarfRegAlias<R6 >;
  def LN : EtcaReg<7,  "ln", [R7]         >, DwarfRegAlias<R7 >;

/*
  def T0 : EtcaReg<8,  "t0", [R8]         >, DwarfRegAlias<R8 >;
  def T1 : EtcaReg<9,  "t1", [R9]         >, DwarfRegAlias<R9 >;
  def T2 : EtcaReg<10, "t2", [R10]        >, DwarfRegAlias<R10>;
  def T3 : EtcaReg<11, "t3", [R11]        >, DwarfRegAlias<R11>;
  def T4 : EtcaReg<12, "t4", [R12]        >, DwarfRegAlias<R12>;
  def S2 : EtcaReg<13, "s2", [R13]        >, DwarfRegAlias<R13>;
  def S3 : EtcaReg<14, "s3", [R14]        >, DwarfRegAlias<R14>;
  def S4 : EtcaReg<15, "s4", [R15]        >, DwarfRegAlias<R15>;
*/
}

// virtual flags register 
def FR : EtcaReg<16, "fr">;

// Register classes.
def GPR : RegisterClass<"Etca", [i16], 16,
    (add R0, R1, R2,  R3,  R4,  R5,  R6,  R7,
         /*R8, R9, R10, R11, R12, R13, R14, R15,*/
         // Non-allocatable regs: 
         FR
    )>;

