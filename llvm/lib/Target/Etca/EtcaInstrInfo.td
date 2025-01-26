//====-- EtcaInstrInfo.td - Target Description for Etca Target -----------====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Etca instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Instruction format superclass
//===----------------------------------------------------------------------===//

include "EtcaInstrFormats.td"

// -------------------------------------------------- //
// Instruction Patterns
// -------------------------------------------------- //

// TODO: they all affect flags!

// a = a op b
class ArithLogic_RR<bits<4> opcode, string instrAsm,
      SDPatternOperator opNode>
  : InstRR<opcode, (outs GPR:$Ra), (ins GPR:$va, GPR:$Rb),
           instrAsm#"\t$Ra, $Rb",
           [(set GPR:$Ra, (opNode GPR:$va, GPR:$Rb))]> {
  let ss = 0b01;
  let Constraints = "$Ra = $va";
}

// a = a op b
class ArithLogic_RI<bits<4> opcode, string instrAsm,
      SDPatternOperator opNode>
  : InstRI<opcode, (outs GPR:$Ra), (ins GPR:$va, Imm5:$imm5),
           instrAsm#"\t$Ra, $imm5",
           [(set GPR:$Ra, (opNode GPR:$va, Imm5:$imm5))]> {
  let ss = 0b01;
  let Constraints = "$Ra = $va";
}

// a = b op a
class ArithLogic_Rev_RR<bits<4> opcode, string instrAsm,
      SDPatternOperator opNode>
  : InstRR<opcode, (outs GPR:$Ra), (ins GPR:$va, GPR:$Rb),
           instrAsm#"\t$Ra, $Rb",
           [(set GPR:$Ra, (opNode GPR:$Rb, GPR:$va))]> {
  let ss = 0b01;
  let Constraints = "$Ra = $va";
}

// a = b op a
class ArithLogic_Rev_RI<bits<4> opcode, string instrAsm,
      SDPatternOperator opNode>
  : InstRI<opcode, (outs GPR:$Ra), (ins GPR:$va, Imm5:$imm5),
           instrAsm#"\t$Ra, $imm5",
           [(set GPR:$Ra, (opNode Imm5:$imm5, GPR:$va))]> {
  let ss = 0b01;
  let Constraints = "$Ra = $va";
}

// a = b
class Move_RR<bits<4> opcode, string instrAsm>
  : InstRR<opcode, (outs GPR:$Ra), (ins GPR:$Rb),
           instrAsm#"\t$Ra, $Rb",
           [(set GPR:$Ra, GPR:$Rb)]> {
  let ss = 0b01;
  let isMoveReg = 1;
}

// a = b
class Move_RI<bits<4> opcode, string instrAsm>
  : InstRI<opcode, (outs GPR:$Ra), (ins Imm5:$imm5),
           instrAsm#"\t$Ra, $imm5",
           [(set GPR:$Ra, Imm5:$imm5)]> {
  let ss = 0b01;
  let isMoveImm = 1;
}

// a = *b
class Load_RR<bits<4> opcode, string instrAsm>
  : InstRR<opcode, (outs GPR:$Ra), (ins GPR:$Rb),
           instrAsm#"\t$Ra, $Rb",
           [(set GPR:$Ra, (load GPR:$Rb))]> {
  let ss = 0b01;
  let mayLoad = 1;
}

// a = *b
class Load_RI<bits<4> opcode, string instrAsm>
  : InstRI<opcode, (outs GPR:$Ra), (ins Imm5:$imm5),
           instrAsm#"\t$Ra, $imm5",
           [(set GPR:$Ra, (load Imm5:$imm5))]> {
  let ss = 0b01;
  let mayLoad = 1;
}

// -------------------------------------------------- //
// Instructions
// -------------------------------------------------- //

def JMP_REG : InstBR_Reg<(outs), (ins GPR:$reg), "jmp", []> {
    let isBranch = 1;
    let isTerminator = 1;
    let cc = 0b1110;
}

def ADD_RR : ArithLogic_RR<0b0000, "add", add>;
def ADD_RI : ArithLogic_RI<0b0000, "add", add>;

def SUB_RR : ArithLogic_RR<0b0001, "sub", sub>;
def SUB_RI : ArithLogic_RI<0b0001, "sub", sub>;

def RSUB_RR : ArithLogic_Rev_RR<0b0010, "rsub", sub>;
def RSUB_RI : ArithLogic_Rev_RI<0b0010, "rsub", sub>;

// TODO: cmp 

def OR_RR : ArithLogic_RR<0b0100, "or", or>;
def OR_RI : ArithLogic_RI<0b0100, "or", or>;

def XOR_RR : ArithLogic_RR<0b0101, "xor", xor>;
def XOR_RI : ArithLogic_RI<0b0101, "xor", xor>;

def AND_RR : ArithLogic_RR<0b0110, "and", and>;
def AND_RI : ArithLogic_RI<0b0110, "and", and>;

// TODO: test

def MOVZ_RR : Move_RR<0b1000, "movz">;
def MOVZ_RI : Move_RI<0b1000, "movz">;

def MOVS_RR : Move_RR<0b1001, "movs">;
def MOVS_RI : Move_RI<0b1001, "movs">;

def LOAD_RR : Load_RR<0b1010, "load">;
def LOAD_RI : Load_RI<0b1010, "load">;

def STORE_RR : ArithLogic_RR<0b1011, "store", store>;
def STORE_RI : ArithLogic_RI<0b1011, "store", store>;
