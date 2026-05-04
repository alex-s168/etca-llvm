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
// ETCA-specific SDNodes (used for lowering glue/flag results)
//===----------------------------------------------------------------------===//

def SDT_ETCACmp : SDTypeProfile<0, 2, [SDTCisSameAs<0, 1>, SDTCisVT<0, i16>]>;
def ETCAcmp : SDNode<"ETCAISD::CMP", SDT_ETCACmp, [SDNPOutGlue]>;

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
// Each processor model specifies a word + pointer width combination.
// The minimum target is base+saf with 16-bit word + 16-bit pointer.
//===----------------------------------------------------------------------===//

// Base: 16-bit word, 16-bit pointer, SAF extension
def : ProcessorModel<"generic", ETCASchedModel,
    [Feature16Bit, FeaturePtr16, FeatureSAF]>;

// 32-bit word, 32-bit pointer (DW + DWAS)
def : ProcessorModel<"etca32", ETCASchedModel,
    [Feature32Bit, FeaturePtr32, FeatureSAF, FeatureDW, FeatureDWAS]>;

// 32-bit word, 64-bit pointer (DW + QWAS; QWAS requires QW, meaning
// registers are 64-bit but word size is 32 for default operations)
def : ProcessorModel<"etca32p64", ETCASchedModel,
    [Feature64Bit, FeaturePtr64, FeatureSAF, FeatureDW, FeatureQW, FeatureQWAS]>;

// 64-bit word, 32-bit pointer (QW + DWAS; note: DW is implied by QW)
def : ProcessorModel<"etca64p32", ETCASchedModel,
    [Feature64Bit, FeaturePtr32, FeatureSAF, FeatureDW, FeatureQW, FeatureDWAS]>;

// 64-bit word, 64-bit pointer (QW + QWAS)
def : ProcessorModel<"etca64", ETCASchedModel,
    [Feature64Bit, FeaturePtr64, FeatureSAF, FeatureDW, FeatureQW, FeatureQWAS]>;

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
