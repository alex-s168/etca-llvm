# RUN: llvm-mc -arch=etca --disassemble < %s | llvm-mc -arch=etca --show-encoding | FileCheck %s
# RUN: llvm-mc -arch=etca --disassemble < %s | FileCheck --check-prefix=DIS %s

# ==============================================================================
# REX-prefix disassembler roundtrip test.
# Each line is a raw instruction encoding (hex bytes, little-endian).
# REX prefix 0xC0-0xCF extends register fields to 4 bits.
#
# RUN 1: disassemble bytes → text → re-assemble → verify encoding matches
# RUN 2: disassemble bytes → text, verify instruction mnemonic and operands
# ==============================================================================

# === REX 16-bit RR: add %r8, %r9 → [0xc6,0x10,0x04] (REX.A=1, REX.B=1) ===
# REX=0xC6 (bits: 1100 0 A B X = 1100 0 1 1 0), A=1→r8, B=1→r9
0xc6 0x10 0x04
# CHECK: add %r8, %r9              ; encoding: [0xc6,0x10,0x04]
# DIS: add %r8, %r9

# === REX 16-bit RR: sub %r10, %r11 → [0xc6,0x11,0x4c] ===
# AAA=2→r10 (A=1), BBB=3→r11 (B=1)
0xc6 0x11 0x4c
# CHECK: sub %r10, %r11            ; encoding: [0xc6,0x11,0x4c]
# DIS: sub %r10, %r11

# === REX 16-bit RR: and %r12, %r13 → [0xc6,0x16,0x94] ===
# AAA=4→r12 (A=1), BBB=5→r13 (B=1)
0xc6 0x16 0x94
# CHECK: and %r12, %r13            ; encoding: [0xc6,0x16,0x94]
# DIS: and %r12, %r13

# === REX 16-bit RR: or %r14, %r15 → [0xc6,0x14,0xdc] ===
# AAA=6→r14 (A=1), BBB=7→r15 (B=1)
0xc6 0x14 0xdc
# CHECK: or %r14, %r15             ; encoding: [0xc6,0x14,0xdc]
# DIS: or %r14, %r15

# === REX 16-bit RR: xor %r8, %r9 → [0xc6,0x15,0x04] ===
0xc6 0x15 0x04
# CHECK: xor %r8, %r9              ; encoding: [0xc6,0x15,0x04]
# DIS: xor %r8, %r9

# === REX 16-bit untied: movz %r10, %r11 → [0xc6,0x18,0x4c] ===
# MOVZ untied: AAA=dst→r10, BBB=src→r11
0xc6 0x18 0x4c
# CHECK: movz %r10, %r11           ; encoding: [0xc6,0x18,0x4c]
# DIS: movz %r10, %r11

# === REX 16-bit untied: movs %r12, %r13 → [0xc6,0x19,0x94] ===
0xc6 0x19 0x94
# CHECK: movs %r12, %r13           ; encoding: [0xc6,0x19,0x94]
# DIS: movs %r12, %r13

# === REX 16-bit CMP: cmp %r14, %r15 → [0xc6,0x13,0xdc] ===
0xc6 0x13 0xdc
# CHECK: cmp %r14, %r15            ; encoding: [0xc6,0x13,0xdc]
# DIS: cmp %r14, %r15

# === REX 16-bit TEST: test %r8, %r9 → [0xc6,0x17,0x04] ===
0xc6 0x17 0x04
# CHECK: test %r8, %r9             ; encoding: [0xc6,0x17,0x04]
# DIS: test %r8, %r9

# === REX LOAD: load %r8, %r9 → [0xc6,0x1a,0x04] ===
0xc6 0x1a 0x04
# CHECK: load %r8, %r9             ; encoding: [0xc6,0x1a,0x04]
# DIS: load %r8, %r9

# === REX STORE: store %r10, %r11 → [0xc6,0x1b,0x4c] ===
0xc6 0x1b 0x4c
# CHECK: store %r10, %r11          ; encoding: [0xc6,0x1b,0x4c]
# DIS: store %r10, %r11

# === REX 32-bit RR: add %r8d, %r9d → [0xc6,0x20,0x04] ===
0xc6 0x20 0x04
# CHECK: add %r8d, %r9d            ; encoding: [0xc6,0x20,0x04]
# DIS: add %r8d, %r9d

# === REX 32-bit RR: sub %r10d, %r11d → [0xc6,0x21,0x4c] ===
0xc6 0x21 0x4c
# CHECK: sub %r10d, %r11d          ; encoding: [0xc6,0x21,0x4c]
# DIS: sub %r10d, %r11d

# === REX 32-bit untied: movz %r12d, %r13d → [0xc6,0x28,0x94] ===
0xc6 0x28 0x94
# CHECK: movz %r12d, %r13d         ; encoding: [0xc6,0x28,0x94]
# DIS: movz %r12d, %r13d

# === REX 64-bit RR: add %r8q, %r9q → [0xc6,0x30,0x04] ===
0xc6 0x30 0x04
# CHECK: add %r8q, %r9q            ; encoding: [0xc6,0x30,0x04]
# DIS: add %r8q, %r9q

# === REX 64-bit RR: sub %r10q, %r11q → [0xc6,0x31,0x4c] ===
0xc6 0x31 0x4c
# CHECK: sub %r10q, %r11q          ; encoding: [0xc6,0x31,0x4c]
# DIS: sub %r10q, %r11q

# === REX 64-bit untied: movz %r12q, %r13q → [0xc6,0x38,0x94] ===
0xc6 0x38 0x94
# CHECK: movz %r12q, %r13q         ; encoding: [0xc6,0x38,0x94]
# DIS: movz %r12q, %r13q

# === REX RI: add %r8, 7 → [0xc6,0x50,0x07] ===
# AAA=0→r8 (A=1), imm=7
0xc6 0x50 0x07
# CHECK: add %r8, 7                ; encoding: [0xc6,0x50,0x07]
# DIS: add %r8, 7

# === REX RI: sub %r10, 5 → [0xc6,0x51,0x45] ===
# AAA=2→r10 (A=1), imm=5
0xc6 0x51 0x45
# CHECK: sub %r10, 5               ; encoding: [0xc6,0x51,0x45]
# DIS: sub %r10, 5

# === REX RI (untied): movz %r14, 31 → [0xc4,0x58,0xdf] ===
# REX=0xC4 (A=1, B=0) → AAA=6|(1<<3)=14→r14, imm=31=0x1f
# But wait, byte 2 is 0xdf, not 0x1f. 0xdf in AAA|IIIII format:
#   0xdf = 0b11011111 = AAA=6 | IIIII=31
# REX.A=1 extends AAA=6 to r14. B=0, so no BBB extension.
0xc4 0x58 0xdf
# CHECK: movz %r14, 31             ; encoding: [0xc4,0x58,0xdf]
# DIS: movz %r14, 31

# === REX PUSH: push %r8 → [0xc2,0x1d,0xc0] ===
# REX=0xC2 (A=1, B=0) → PUSH uses BBB field: BBB=0→r8(B=0→r0... no)
# Actually PUSH: Inst{15-13}=sp=6, Inst{12-10}=src
# BBB=0, REX.B=0 → r0. But we want r8.
# Wait, REX=0xC2: bit 2(A)=0, bit 1(B)=1
# So B=1, BBB=0 → r0|(1<<3)=8 → r8 ✓
0xc2 0x1d 0xc0
# CHECK: push %r8                  ; encoding: [0xc2,0x1d,0xc0]
# DIS: push %r8

# === REX POP: pop %r9 → [0xc4,0x1c,0x38] ===
# REX=0xC4 (A=1, B=0)
# POP uses AAA: Inst{15-13}=dst, Inst{12-10}=sp
# AAA=7→r7, A=1 → r7|8=15. But we want r9.
# Hmm, AAA=1→r1, A=1 → r1|8=9 → r9 ✓
# So byte1 high nibble = 0x3 = AAA=1 | BBB=sp=6 | 0
0xc4 0x1c 0x38
# CHECK: pop %r9                   ; encoding: [0xc4,0x1c,0x38]
# DIS: pop %r9

# === REX with only A bit: movz %r8, %r0 → REX.A=1, REX.B=0 ===
# movz %r8, %r0: AAA=0→r0+8=r8 (A=1), BBB=0→r0 (B=0)
# REX=0xC4 (A=1, B=0), encoded as 0xC4 0x18 0x00
0xc4 0x18 0x00
# CHECK: movz %r8, %r0             ; encoding: [0xc4,0x18,0x00]
# DIS: movz %r8, %r0

# === REX with only B bit: movz %r0, %r8 → REX.A=0, REX.B=1 ===
# movz %r0, %r8: AAA=0→r0 (A=0), BBB=0→r0+8=r8 (B=1)
# REX=0xC2 (A=0, B=1), encoded as 0xC2 0x18 0x00
0xc2 0x18 0x00
# CHECK: movz %r0, %r8             ; encoding: [0xc2,0x18,0x00]
# DIS: movz %r0, %r8
