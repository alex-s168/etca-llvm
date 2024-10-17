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

// a = a op b
class ArithLogic_RR<bits<4> opcode, string instrAsm,
      SDPatternOperator opNode>
  : InstRR<opcode, (outs AR:$a), (ins AR:$a, AR:$b),
           instrAsm#"\t$a, $b",
           [(set AR:$a, (opNode AR:$a, AR:$b))]> {
}

// a = a op b
class ArithLogic_RI<bits<4> opcode, string instrAsm,
      SDPatternOperator opNode>
  : InstRI<opcode, (outs AR:$a), (ins AR:$a, imm5:$b),
           instrAsm#"\t$a, $b",
           [(set AR:$a, (opNode AR:$a, imm5:$b))]> {
}

// a = b op a
class ArithLogic_Rev_RR<bits<4> opcode, string instrAsm,
      SDPatternOperator opNode>
  : InstRR<opcode, (outs AR:$a), (ins AR:$a, AR:$b),
           instrAsm#"\t$a, $b",
           [(set AR:$a, (opNode AR:$b, AR:$a))]> {
}

// a = b op a
class ArithLogic_Rev_RI<bits<4> opcode, string instrAsm,
      SDPatternOperator opNode>
  : InstRI<opcode, (outs AR:$a), (ins AR:$a, imm5:$b),
           instrAsm#"\t$a, $b",
           [(set AR:$a, (opNode imm5:$b, AR:$a))]> {
}

// a = b
class Move_RR<bits<4> opcode, string instrAsm>
  : InstRR<opcode, (outs AR:$d), (ins AR:$s),
           instrAsm#"\t$d, $s",
           [(set AR:$d, AR:$s)]> {
}

// a = b
class Move_RI<bits<4> opcode, string instrAsm>
  : InstRI<opcode, (outs AR:$d), (ins imm5:$s),
           instrAsm#"\t$d, $s",
           [(set AR:$d, imm5:$s)]> {
}

// -------------------------------------------------- //
// ALU & Move Instructions
// -------------------------------------------------- //

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

def LOAD_RR : ArithLogic_RR<0b1010, "load", load>;
def LOAD_RI : ArithLogic_RI<0b1010, "load", load>;

def STORE_RR : ArithLogic_RR<0b1011, "store", store>;
def STORE_RI : ArithLogic_RI<0b1011, "store", store>;
