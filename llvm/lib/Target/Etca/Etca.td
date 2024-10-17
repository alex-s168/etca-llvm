//====- Etca.td - Describe the Etca Target Machine --------*- tablegen -*-====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces which we are implementing
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Register File, Calling Conv, Instruction Descriptions
//===----------------------------------------------------------------------===//

include "EtcaSchedule.td"
include "EtcaRegisterInfo.td"
include "EtcaCallingConv.td"
include "EtcaInstrInfo.td"

def EtcaInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Etca processors supported.
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic", EtcaSchedModel, []>;

def EtcaInstPrinter : AsmWriter {
  string AsmWriterClassName  = "InstPrinter";
  bit isMCAsmWriter = 1;
}

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def Etca : Target {
  // Pull in Instruction Info:
  let InstructionSet = EtcaInstrInfo;
  let AssemblyWriters = [EtcaInstPrinter];
}
