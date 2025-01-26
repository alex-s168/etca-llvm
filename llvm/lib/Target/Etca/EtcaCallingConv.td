//===- EtcaCallingConv.td - Calling Conventions Etca -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the Etca architecture.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Return Value Calling Conventions
//===----------------------------------------------------------------------===//

// Etca 32-bit C Calling convention.
def CC_Etca : CallingConv<[
  // Put argument in registers if marked 'inreg' and not a vararg call.
  CCIfNotVarArg<CCIfInReg<CCIfType<[i8, i16, i32],
                                   CCAssignToReg<[A0, A1]>>>>,

  // Otherwise they are assigned to the stack in 2-byte aligned units.

  CCIfType<[i8, i16, i32],
           CCAssignToStack<2, 4>>,
]>;

def CSR: CalleeSavedRegs<(add S0, S1, BP, SP /*, S2, S3, S4*/)>;
