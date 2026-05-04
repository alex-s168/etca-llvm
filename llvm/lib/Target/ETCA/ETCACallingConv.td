//===- ETCACallingConv.td - ETCA Calling Conventions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//  Describe the ETCA calling convention
//
//  ETCA SAF ABI:
//    - r0-r1: argument/return value registers (caller-saved)
//    - r2:    argument register (caller-saved)
//    - r3-r4: callee-saved registers
//    - r5:    base pointer (bp) — callee-saved
//    - r6:    stack pointer (sp) — callee-saved
//    - r7:    link register (ln) — caller-saved
//
//  Arguments: first 4 in r0-r3, rest on stack (aligned to register width)
//  Return:    r0 (16/32-bit) or r0-r1 pair (64-bit)
//
//  Stack grows down.  Stack is aligned to register width bytes.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Calling convention for formal arguments and call operands
//===----------------------------------------------------------------------===//

def CC_ETCA : CallingConv<[
  // Promote i8/i16 to i32 when word size >= 32
  CCIfType<[i8, i16], CCPromoteToType<i32>>,

  // First 4 arguments in registers r0-r3
  CCIfType<[i16], CCAssignToReg<[R0, R1, R2, R3]>>,
  CCIfType<[i32], CCAssignToReg<[D0, D1, D2, D3]>>,
  CCIfType<[i64], CCAssignToReg<[Q0, Q1, Q2, Q3]>>,

  // Remaining arguments on stack, 2-byte aligned for i16, 4 for i32, 8 for i64
  CCIfType<[i16], CCAssignToStack<2, 2>>,
  CCIfType<[i32], CCAssignToStack<4, 4>>,
  CCIfType<[i64], CCAssignToStack<8, 8>>
]>;

//===----------------------------------------------------------------------===//
// Calling convention for return values
//===----------------------------------------------------------------------===//

def RetCC_ETCA : CallingConv<[
  // Promote i8/i16 returns to i32 when word size >= 32
  CCIfType<[i8, i16], CCPromoteToType<i32>>,

  // Return values in r0 or r0-r1 pair
  CCIfType<[i16], CCAssignToReg<[R0]>>,
  CCIfType<[i32], CCAssignToReg<[D0]>>,
  CCIfType<[i64], CCAssignToReg<[Q0, Q1]>>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved registers (CSR) masks
//
// SAF ABI callee-saved: R3, R4, R5 (bp), R6 (sp)
//===----------------------------------------------------------------------===//

def CSR_ETCA_RegMask : CalleeSavedRegs<(add R3, R4, R5, R6)>;
