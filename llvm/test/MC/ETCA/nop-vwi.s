# RUN: llvm-mc -arch=etca -mattr=+vwi -show-encoding < %s | FileCheck %s
# RUN: llvm-mc -arch=etca -mattr=+vwi -filetype=obj < %s -o /dev/null

# VWI extension adds the single-byte NOP (0xAE) instruction.
# The nop mnemonic still produces the 2-byte NOP (0x008F) for backward
# compatibility with the base ISA.

nop
# CHECK: nop                       ; encoding: [0x8f,0x00]

# REX implies VWI, so -mattr=+rex should also enable VWI features
# RUN: llvm-mc -arch=etca -mattr=+rex -show-encoding < %s | FileCheck %s --check-prefix=REX

# REX enables VWI, but nop still uses 2-byte encoding
nop
# REX: nop                       ; encoding: [0x8f,0x00]
