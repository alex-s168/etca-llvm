# RUN: llvm-mc -arch=etca -show-encoding < %s | FileCheck %s

# NOP = 0x008F → LE [0x8f,0x00]
nop
# CHECK: nop                       ; encoding: [0x8f,0x00]

# Multiple NOPs in sequence
nop
nop
# CHECK: nop                       ; encoding: [0x8f,0x00]
# CHECK: nop                       ; encoding: [0x8f,0x00]
