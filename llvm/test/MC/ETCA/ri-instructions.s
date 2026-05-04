# RUN: llvm-mc -arch=etca -show-encoding < %s | FileCheck %s

# Test all register-immediate ALU instructions with 16-bit registers.
# RI format: (rA << 13) | (imm << 8) | (0x01 << 6) | (SS << 4) | CCCC

# --- Basic RI instructions (each with r0, imm=5) ---

# add %r0, 5 → 0x0550 → [0x50,0x05]
add %r0, 5
# CHECK: add %r0, 5                ; encoding: [0x50,0x05]

# sub %r0, 5 → 0x0551 → [0x51,0x05]
sub %r0, 5
# CHECK: sub %r0, 5                ; encoding: [0x51,0x05]

# rsub %r0, 5 → 0x0552 → [0x52,0x05]
rsub %r0, 5
# CHECK: rsub %r0, 5               ; encoding: [0x52,0x05]

# or %r0, 5 → 0x0554 → [0x54,0x05]
or %r0, 5
# CHECK: or %r0, 5                 ; encoding: [0x54,0x05]

# xor %r0, 5 → 0x0555 → [0x55,0x05]
xor %r0, 5
# CHECK: xor %r0, 5                ; encoding: [0x55,0x05]

# and %r0, 5 → 0x0556 → [0x56,0x05]
and %r0, 5
# CHECK: and %r0, 5                ; encoding: [0x56,0x05]

# movz %r0, 5 → 0x0558 → [0x58,0x05]
movz %r0, 5
# CHECK: movz %r0, 5               ; encoding: [0x58,0x05]

# movs %r0, 5 → 0x0559 → [0x59,0x05]
movs %r0, 5
# CHECK: movs %r0, 5               ; encoding: [0x59,0x05]

# cmp %r0, 5 → 0x0553 → [0x53,0x05]
cmp %r0, 5
# CHECK: cmp %r0, 5                ; encoding: [0x53,0x05]

# test %r0, 5 → 0x0557 → [0x57,0x05]
test %r0, 5
# CHECK: test %r0, 5               ; encoding: [0x57,0x05]

# slo %r0, 5 → 0x055C → [0x5c,0x05]
slo %r0, 5
# CHECK: slo %r0, 5                ; encoding: [0x5c,0x05]

# --- Imm5 boundary tests ---

# Maximum signed positive (15): 5-bit imm = 01111
add %r0, 15
# CHECK: add %r0, 15               ; encoding: [0x50,0x0f]

# Maximum unsigned (31): 5-bit imm = 11111
movz %r0, 31
# CHECK: movz %r0, 31              ; encoding: [0x58,0x1f]

# Zero immediate
add %r0, 0
# CHECK: add %r0, 0                ; encoding: [0x50,0x00]

# --- Different register ---

# add %r5, 3 → 0xA350 → [0x50,0xa3]
add %r5, 3
# CHECK: add %r5, 3                ; encoding: [0x50,0xa3]

# and %r7, 31 → 0xFF56 → [0x56,0xff]
and %r7, 31
# CHECK: and %r7, 31               ; encoding: [0x56,0xff]

# --- 32-bit RI (dword registers: %rNd) ---
# add %r0d, 5 → 0x0560 → [0x60,0x05]
add %r0d, 5
# CHECK: add %r0d, 5               ; encoding: [0x60,0x05]

# --- 64-bit RI (qword registers: %rNq) ---
# add %r0q, 5 → 0x0570 → [0x70,0x05]
add %r0q, 5
# CHECK: add %r0q, 5               ; encoding: [0x70,0x05]

# --- READCR / WRITECR ---
# readcr %r0, 3 → 0x035E → [0x5e,0x03]
readcr %r0, 3
# CHECK: readcr %r0, 3             ; encoding: [0x5e,0x03]

# writecr %r0, 7 → 0x075F → [0x5f,0x07]
writecr %r0, 7
# CHECK: writecr %r0, 7            ; encoding: [0x5f,0x07]
