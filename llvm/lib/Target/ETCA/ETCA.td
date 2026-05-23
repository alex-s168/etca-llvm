//===- ETCA.td - Describe the ETCA Target Machine ----------*- tablegen -*-===//
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
// Subtarget Features
//===----------------------------------------------------------------------===//

// Word size: determines physical register width
def Feature16Bit : SubtargetFeature<"16bit", "WordSize", "16",
    "16-bit word size (base ISA)">;
def Feature32Bit : SubtargetFeature<"32bit", "WordSize", "32",
    "32-bit word size (DW extension)">;
def Feature64Bit : SubtargetFeature<"64bit", "WordSize", "64",
    "64-bit word size (QW extension)">;

// Pointer size: determines address register width
def FeaturePtr16 : SubtargetFeature<"ptr16", "PtrSize", "16",
    "16-bit pointer size">;
def FeaturePtr32 : SubtargetFeature<"ptr32", "PtrSize", "32",
    "32-bit pointer size (DWAS extension)">;
def FeaturePtr64 : SubtargetFeature<"ptr64", "PtrSize", "64",
    "64-bit pointer size (QWAS extension)">;

// Extension features — enable specific ISA extensions
// Base is always enabled.
def FeatureSAF : SubtargetFeature<"saf", "HasSAF", "true",
    "Stack and Functions extension">;
def FeatureDW  : SubtargetFeature<"dw", "HasDW", "true",
    "32-bit Doubleword Operations extension">;
def FeatureQW  : SubtargetFeature<"qw", "HasQW", "true",
    "64-bit Quadword Operations extension">;
def FeatureByte : SubtargetFeature<"byte", "HasByte", "true",
    "8-bit Byte Operations extension (SS=00)">;
def FeatureREX : SubtargetFeature<"rex", "HasREX", "true",
    "Expanded Registers extension (REX prefix, r8-r15)">;
def FeatureDWAS : SubtargetFeature<"dwas", "HasDWAS", "true",
    "32-bit Address Space extension">;
def FeatureQWAS : SubtargetFeature<"qwas", "HasQWAS", "true",
    "64-bit Address Space extension">;

//===----------------------------------------------------------------------===//
// Schedule Model (must be before ProcessorModel references)
//===----------------------------------------------------------------------===//

include "ETCASchedule.td"

//===----------------------------------------------------------------------===//
// ETCA processors supported.
//
// The ETCA backend uses a single "generic" CPU model.  Word size, pointer
// size, and extensions are selected via -mattr feature flags:
//   Word size:  +16bit | +32bit | +64bit
//   Ptr size:   +ptr16 | +ptr32 | +ptr64
//   Extensions: +dw, +qw, +byte, +saf, +rex, +dwas, +qwas
//
// Example: 64-bit word + 64-bit pointer → -mattr=+64bit,+ptr64,+dw,+qw
// Example: 32-bit word + 32-bit pointer → -mattr=+32bit,+ptr32,+dw
// Example: 64-bit word + 32-bit pointer → -mattr=+64bit,+ptr32,+dw,+qw
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic", ETCASchedModel,
    [Feature16Bit, FeaturePtr16, FeatureSAF, FeatureByte]>;

//===----------------------------------------------------------------------===//
// Register File, Calling Conv, Instruction Descriptions
//===----------------------------------------------------------------------===//

include "ETCARegisterInfo.td"
include "ETCARegisterBanks.td"
include "ETCACallingConv.td"
include "ETCAInstrInfo.td"

def ETCAInstrInfo : InstrInfo;

// Remap pseudo source operands for GPR
defm : RemapAllTargetPseudoPointerOperands<GPR>;

//===----------------------------------------------------------------------===//
// Assembly Printers and Parsers
//===----------------------------------------------------------------------===//

def ETCAInstPrinter : AsmWriter {
  string AsmWriterClassName  = "InstPrinter";
  bit isMCAsmWriter = 1;
}

def ETCAAsmParser : AsmParser {
  let AllowDuplicateRegisterNames = 1;
}

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def ETCA : Target {
  let InstructionSet = ETCAInstrInfo;
  let AssemblyWriters = [ETCAInstPrinter];
  let AssemblyParsers = [ETCAAsmParser];
}
