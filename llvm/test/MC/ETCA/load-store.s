# RUN: llvm-mc -arch=etca -show-encoding < %s | FileCheck %s

# Test LOAD/STORE in RR format: (rA << 13) | (rB << 10) | (SS << 4) | CCCC
# LOAD: CCCC=0xA (1010), STORE: CCCC=0xB (1011)

# --- LOAD/STORE word (16-bit) ---
# load %r0, %r1 → 0x041A → [0x1a,0x04]
load %r0, %r1
# CHECK: load %r0, %r1              ; encoding: [0x1a,0x04]

# store %r0, %r1 → 0x041B → [0x1b,0x04]
store %r0, %r1
# CHECK: store %r0, %r1             ; encoding: [0x1b,0x04]

# --- Different registers ---
# load %r3, %r5 → 0x741A → [0x1a,0x74]
load %r3, %r5
# CHECK: load %r3, %r5              ; encoding: [0x1a,0x74]

# store %r7, %r2 → 0xE81B → [0x1b,0xe8]
store %r7, %r2
# CHECK: store %r7, %r2             ; encoding: [0x1b,0xe8]

# load %r5, %r4 → 0xB01A → [0x1a,0xb0]
load %r5, %r4
# CHECK: load %r5, %r4              ; encoding: [0x1a,0xb0]

# --- CMP/TEST in RR format (word) ---
# cmp %r0, %r1 → 0x0413 → [0x13,0x04]
cmp %r0, %r1
# CHECK: cmp %r0, %r1               ; encoding: [0x13,0x04]

# test %r0, %r1 → 0x0417 → [0x17,0x04]
test %r0, %r1
# CHECK: test %r0, %r1              ; encoding: [0x17,0x04]

# cmp %r6, %r3 → 0xCC13 → [0x13,0xcc]
cmp %r6, %r3
# CHECK: cmp %r6, %r3               ; encoding: [0x13,0xcc]

# test %r7, %r0 → 0xE017 → [0x17,0xe0]
test %r7, %r0
# CHECK: test %r7, %r0              ; encoding: [0x17,0xe0]

# --- mov psedo-instruction (maps to movz/movzi, or lowers to LOAD/STORE) ---
# movx %r0, 0 → MOVZI16 rA=0, imm=0 → 0x0058 → [0x58,0x00]
movx %r0, 0
# CHECK: movz %r0, 0                ; encoding: [0x58,0x00]

# movx [%r1], %r0 → STORE16 rA=0, rB=1 → 0x041B → [0x1b,0x04]
movx [%r1], %r0
# CHECK: store %r0, %r1             ; encoding: [0x1b,0x04]

# movx %r2, [%r1] → LOAD16 rA=2, rB=1 → 0x441A → [0x1a,0x44]
movx %r2, [%r1]
# CHECK: load %r2, %r1             ; encoding: [0x1a,0x44]

# mov %r0, 15 → MOVZI16 rA=0, imm=15 → 0x0F58 → [0x58,0x0f]
mov %r0, 15
# CHECK: movz %r0, 15               ; encoding: [0x58,0x0f]

# movx %r5, %r3 → MOVZ16 rA=5, rB=3 → 0xAC18 → [0x18,0xac]
movx %r5, %r3
# CHECK: movz %r5, %r3              ; encoding: [0x18,0xac]
