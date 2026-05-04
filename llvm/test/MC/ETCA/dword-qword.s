# RUN: llvm-mc -arch=etca -show-encoding < %s | FileCheck %s

# ==============================================================================
# Dword (32-bit) and Qword (64-bit) instruction encoding verification.
#
# ETCa SS bits in instruction encoding:
#   SS=01 → word (16-bit, base ISA)   — %rN / %rNx
#   SS=10 → dword (32-bit, DW ext.)   — %rNd
#   SS=11 → qword (64-bit, QW ext.)   — %rNq
#
# RR format (byte0 = (SS << 4) | CCCC):
#   word  (SS=01): byte0 = 0x10 | CCCC
#   dword (SS=10): byte0 = 0x20 | CCCC
#   qword (SS=11): byte0 = 0x30 | CCCC
#   byte1 = (rA << 5) | (rB << 2) (MM=00 for base)
#
# RI format (byte0 = 0x40 | (SS << 4) | CCCC):
#   word  (SS=01): byte0 = 0x50 | CCCC
#   dword (SS=10): byte0 = 0x60 | CCCC
#   qword (SS=11): byte0 = 0x70 | CCCC
#   byte1 = (rA << 5) | imm (5-bit signed)
#
# NOTE:
#   CMP/TEST RR: use width-specific encoding based on register suffix.
#   CMP/TEST RI: use width-specific encoding based on register suffix.
#   SLO, READCR, WRITECR: always use word (SS=01) encoding regardless of suffix.
# ==============================================================================


# ==================== DWORD RR Instructions (SS=10) ====================
# ADD32:   CCCC=0000 → byte0=0x20
add %r0d, %r1d
# CHECK: add %r0d, %r1d            ; encoding: [0x20,0x04]
add %r3d, %r5d
# CHECK: add %r3d, %r5d            ; encoding: [0x20,0x74]
add %r7d, %r7d
# CHECK: add %r7d, %r7d            ; encoding: [0x20,0xfc]

# SUB32:   CCCC=0001 → byte0=0x21
sub %r0d, %r1d
# CHECK: sub %r0d, %r1d            ; encoding: [0x21,0x04]
sub %r3d, %r5d
# CHECK: sub %r3d, %r5d            ; encoding: [0x21,0x74]
sub %r7d, %r0d
# CHECK: sub %r7d, %r0d            ; encoding: [0x21,0xe0]

# RSUB32:  CCCC=0010 → byte0=0x22
rsub %r0d, %r1d
# CHECK: rsub %r0d, %r1d           ; encoding: [0x22,0x04]
rsub %r5d, %r2d
# CHECK: rsub %r5d, %r2d           ; encoding: [0x22,0xa8]

# CMP32 RR: SS=10, CCCC=0011
cmp %r0d, %r1d
# CHECK: cmp %r0d, %r1d            ; encoding: [0x23,0x04]
cmp %r6d, %r3d
# CHECK: cmp %r6d, %r3d            ; encoding: [0x23,0xcc]

# OR32:    CCCC=0100 → byte0=0x24
or %r0d, %r1d
# CHECK: or %r0d, %r1d             ; encoding: [0x24,0x04]
or %r5d, %r2d
# CHECK: or %r5d, %r2d             ; encoding: [0x24,0xa8]

# XOR32:   CCCC=0101 → byte0=0x25
xor %r0d, %r1d
# CHECK: xor %r0d, %r1d            ; encoding: [0x25,0x04]
xor %r6d, %r4d
# CHECK: xor %r6d, %r4d            ; encoding: [0x25,0xd0]

# AND32:   CCCC=0110 → byte0=0x26
and %r0d, %r1d
# CHECK: and %r0d, %r1d            ; encoding: [0x26,0x04]
and %r1d, %r7d
# CHECK: and %r1d, %r7d            ; encoding: [0x26,0x3c]

# TEST32 RR: SS=10, CCCC=0111
test %r0d, %r1d
# CHECK: test %r0d, %r1d           ; encoding: [0x27,0x04]
test %r7d, %r0d
# CHECK: test %r7d, %r0d           ; encoding: [0x27,0xe0]

# MOVZ32:  CCCC=1000 → byte0=0x28
movz %r0d, %r1d
# CHECK: movz %r0d, %r1d           ; encoding: [0x28,0x04]
movz %r7d, %r6d
# CHECK: movz %r7d, %r6d           ; encoding: [0x28,0xf8]

# MOVS32:  CCCC=1001 → byte0=0x29
movs %r0d, %r1d
# CHECK: movs %r0d, %r1d           ; encoding: [0x29,0x04]
movs %r6d, %r1d
# CHECK: movs %r6d, %r1d           ; encoding: [0x29,0xc4]

# LOAD32:  CCCC=1010 → byte0=0x2a
load %r0d, %r1d
# CHECK: load %r0d, %r1d           ; encoding: [0x2a,0x04]
load %r3d, %r5d
# CHECK: load %r3d, %r5d           ; encoding: [0x2a,0x74]

# STORE32: CCCC=1011 → byte0=0x2b
store %r0d, %r1d
# CHECK: store %r0d, %r1d          ; encoding: [0x2b,0x04]
store %r7d, %r2d
# CHECK: store %r7d, %r2d          ; encoding: [0x2b,0xe8]


# ==================== QWORD RR Instructions (SS=11) ====================
# ADD64:   CCCC=0000 → byte0=0x30
add %r0q, %r1q
# CHECK: add %r0q, %r1q            ; encoding: [0x30,0x04]
add %r3q, %r5q
# CHECK: add %r3q, %r5q            ; encoding: [0x30,0x74]
add %r7q, %r7q
# CHECK: add %r7q, %r7q            ; encoding: [0x30,0xfc]

# SUB64:   CCCC=0001 → byte0=0x31
sub %r0q, %r1q
# CHECK: sub %r0q, %r1q            ; encoding: [0x31,0x04]
sub %r3q, %r5q
# CHECK: sub %r3q, %r5q            ; encoding: [0x31,0x74]
sub %r7q, %r0q
# CHECK: sub %r7q, %r0q            ; encoding: [0x31,0xe0]

# RSUB64:  CCCC=0010 → byte0=0x32
rsub %r0q, %r1q
# CHECK: rsub %r0q, %r1q           ; encoding: [0x32,0x04]
rsub %r5q, %r2q
# CHECK: rsub %r5q, %r2q           ; encoding: [0x32,0xa8]

# CMP64 RR: SS=11, CCCC=0011
cmp %r0q, %r1q
# CHECK: cmp %r0q, %r1q            ; encoding: [0x33,0x04]
cmp %r6q, %r3q
# CHECK: cmp %r6q, %r3q            ; encoding: [0x33,0xcc]

# OR64:    CCCC=0100 → byte0=0x34
or %r0q, %r1q
# CHECK: or %r0q, %r1q             ; encoding: [0x34,0x04]
or %r5q, %r2q
# CHECK: or %r5q, %r2q             ; encoding: [0x34,0xa8]

# XOR64:   CCCC=0101 → byte0=0x35
xor %r0q, %r1q
# CHECK: xor %r0q, %r1q            ; encoding: [0x35,0x04]
xor %r6q, %r4q
# CHECK: xor %r6q, %r4q            ; encoding: [0x35,0xd0]

# AND64:   CCCC=0110 → byte0=0x36
and %r0q, %r1q
# CHECK: and %r0q, %r1q            ; encoding: [0x36,0x04]
and %r1q, %r7q
# CHECK: and %r1q, %r7q            ; encoding: [0x36,0x3c]

# TEST64 RR: SS=11, CCCC=0111
test %r0q, %r1q
# CHECK: test %r0q, %r1q           ; encoding: [0x37,0x04]
test %r7q, %r0q
# CHECK: test %r7q, %r0q           ; encoding: [0x37,0xe0]

# MOVZ64:  CCCC=1000 → byte0=0x38
movz %r0q, %r1q
# CHECK: movz %r0q, %r1q           ; encoding: [0x38,0x04]
movz %r7q, %r6q
# CHECK: movz %r7q, %r6q           ; encoding: [0x38,0xf8]

# MOVS64:  CCCC=1001 → byte0=0x39
movs %r0q, %r1q
# CHECK: movs %r0q, %r1q           ; encoding: [0x39,0x04]
movs %r6q, %r1q
# CHECK: movs %r6q, %r1q           ; encoding: [0x39,0xc4]

# LOAD64:  CCCC=1010 → byte0=0x3a
load %r0q, %r1q
# CHECK: load %r0q, %r1q           ; encoding: [0x3a,0x04]
load %r3q, %r5q
# CHECK: load %r3q, %r5q           ; encoding: [0x3a,0x74]

# STORE64: CCCC=1011 → byte0=0x3b
store %r0q, %r1q
# CHECK: store %r0q, %r1q          ; encoding: [0x3b,0x04]
store %r7q, %r2q
# CHECK: store %r7q, %r2q          ; encoding: [0x3b,0xe8]


# ==================== DWORD RI Instructions (SS=10) ====================
# RI byte0 = 0x40 | (SS << 4) | CCCC = 0x60 | CCCC

# ADD32 RI
add %r0d, 5
# CHECK: add %r0d, 5               ; encoding: [0x60,0x05]
add %r3d, -1
# CHECK: add %r3d, -1              ; encoding: [0x60,0x7f]
add %r7d, 15
# CHECK: add %r7d, 15              ; encoding: [0x60,0xef]

# SUB32 RI
sub %r0d, 5
# CHECK: sub %r0d, 5               ; encoding: [0x61,0x05]
sub %r5d, 3
# CHECK: sub %r5d, 3               ; encoding: [0x61,0xa3]

# RSUB32 RI
rsub %r0d, 5
# CHECK: rsub %r0d, 5              ; encoding: [0x62,0x05]
rsub %r3d, -16
# CHECK: rsub %r3d, -16            ; encoding: [0x62,0x70]

# CMP32 RI: CCCC=0011, SS=10 → byte0=0x63
cmp %r0d, 5
# CHECK: cmp %r0d, 5               ; encoding: [0x63,0x05]
cmp %r6d, 7
# CHECK: cmp %r6d, 7               ; encoding: [0x63,0xc7]

# OR32 RI
or %r0d, 5
# CHECK: or %r0d, 5                ; encoding: [0x64,0x05]
or %r7d, 15
# CHECK: or %r7d, 15               ; encoding: [0x64,0xef]

# XOR32 RI
xor %r0d, 5
# CHECK: xor %r0d, 5               ; encoding: [0x65,0x05]
xor %r4d, 7
# CHECK: xor %r4d, 7               ; encoding: [0x65,0x87]

# AND32 RI
and %r0d, 5
# CHECK: and %r0d, 5               ; encoding: [0x66,0x05]
and %r7d, 31
# CHECK: and %r7d, 31              ; encoding: [0x66,0xff]

# TEST32 RI: CCCC=0111, SS=10 → byte0=0x67
test %r0d, 5
# CHECK: test %r0d, 5              ; encoding: [0x67,0x05]
test %r7d, 7
# CHECK: test %r7d, 7              ; encoding: [0x67,0xe7]

# MOVZ32 RI
movz %r0d, 5
# CHECK: movz %r0d, 5              ; encoding: [0x68,0x05]
movz %r0d, 31
# CHECK: movz %r0d, 31             ; encoding: [0x68,0x1f]

# MOVS32 RI
movs %r0d, 5
# CHECK: movs %r0d, 5              ; encoding: [0x69,0x05]
movs %r1d, -8
# CHECK: movs %r1d, -8             ; encoding: [0x69,0x38]

# SLO (word only): SS=01
slo %r0d, 1
# CHECK: slo %r0d, 1               ; encoding: [0x5c,0x01]
slo %r1d, 7
# CHECK: slo %r1d, 7               ; encoding: [0x5c,0x27]

# READCR (word only): SS=01
readcr %r0d, 3
# CHECK: readcr %r0d, 3            ; encoding: [0x5e,0x03]
readcr %r2d, 0
# CHECK: readcr %r2d, 0            ; encoding: [0x5e,0x40]

# WRITECR (word only): SS=01
writecr %r0d, 7
# CHECK: writecr %r0d, 7           ; encoding: [0x5f,0x07]
writecr %r4d, 5
# CHECK: writecr %r4d, 5           ; encoding: [0x5f,0x85]


# ==================== QWORD RI Instructions (SS=11) ====================
# RI byte0 = 0x40 | (SS << 4) | CCCC = 0x70 | CCCC

# ADD64 RI
add %r0q, 5
# CHECK: add %r0q, 5               ; encoding: [0x70,0x05]
add %r3q, -1
# CHECK: add %r3q, -1              ; encoding: [0x70,0x7f]
add %r7q, 15
# CHECK: add %r7q, 15              ; encoding: [0x70,0xef]

# SUB64 RI
sub %r0q, 5
# CHECK: sub %r0q, 5               ; encoding: [0x71,0x05]
sub %r5q, 3
# CHECK: sub %r5q, 3               ; encoding: [0x71,0xa3]

# RSUB64 RI
rsub %r0q, 5
# CHECK: rsub %r0q, 5              ; encoding: [0x72,0x05]
rsub %r3q, -16
# CHECK: rsub %r3q, -16            ; encoding: [0x72,0x70]

# CMP64 RI: CCCC=0011, SS=11 → byte0=0x73
cmp %r0q, 5
# CHECK: cmp %r0q, 5               ; encoding: [0x73,0x05]
cmp %r6q, 7
# CHECK: cmp %r6q, 7               ; encoding: [0x73,0xc7]

# OR64 RI
or %r0q, 5
# CHECK: or %r0q, 5                ; encoding: [0x74,0x05]
or %r7q, 15
# CHECK: or %r7q, 15               ; encoding: [0x74,0xef]

# XOR64 RI
xor %r0q, 5
# CHECK: xor %r0q, 5               ; encoding: [0x75,0x05]
xor %r4q, 7
# CHECK: xor %r4q, 7               ; encoding: [0x75,0x87]

# AND64 RI
and %r0q, 5
# CHECK: and %r0q, 5               ; encoding: [0x76,0x05]
and %r7q, 31
# CHECK: and %r7q, 31              ; encoding: [0x76,0xff]

# TEST64 RI: CCCC=0111, SS=11 → byte0=0x77
test %r0q, 5
# CHECK: test %r0q, 5              ; encoding: [0x77,0x05]
test %r7q, 7
# CHECK: test %r7q, 7              ; encoding: [0x77,0xe7]

# MOVZ64 RI
movz %r0q, 5
# CHECK: movz %r0q, 5              ; encoding: [0x78,0x05]
movz %r0q, 31
# CHECK: movz %r0q, 31             ; encoding: [0x78,0x1f]

# MOVS64 RI
movs %r0q, 5
# CHECK: movs %r0q, 5              ; encoding: [0x79,0x05]
movs %r1q, -8
# CHECK: movs %r1q, -8             ; encoding: [0x79,0x38]

# SLO (word only): SS=01
slo %r0q, 1
# CHECK: slo %r0q, 1               ; encoding: [0x5c,0x01]
slo %r1q, 7
# CHECK: slo %r1q, 7               ; encoding: [0x5c,0x27]

# READCR (word only): SS=01
readcr %r0q, 3
# CHECK: readcr %r0q, 3            ; encoding: [0x5e,0x03]
readcr %r2q, 0
# CHECK: readcr %r2q, 0            ; encoding: [0x5e,0x40]

# WRITECR (word only): SS=01
writecr %r0q, 7
# CHECK: writecr %r0q, 7           ; encoding: [0x5f,0x07]
writecr %r4q, 5
# CHECK: writecr %r4q, 5           ; encoding: [0x5f,0x85]


# ==================== Edge Cases ====================

# All 8 registers with dword
add %r0d, %r0d
# CHECK: add %r0d, %r0d            ; encoding: [0x20,0x00]
add %r1d, %r1d
# CHECK: add %r1d, %r1d            ; encoding: [0x20,0x24]
add %r2d, %r2d
# CHECK: add %r2d, %r2d            ; encoding: [0x20,0x48]
add %r3d, %r3d
# CHECK: add %r3d, %r3d            ; encoding: [0x20,0x6c]
add %r4d, %r4d
# CHECK: add %r4d, %r4d            ; encoding: [0x20,0x90]
add %r5d, %r5d
# CHECK: add %r5d, %r5d            ; encoding: [0x20,0xb4]
add %r6d, %r6d
# CHECK: add %r6d, %r6d            ; encoding: [0x20,0xd8]
add %r7d, %r7d
# CHECK: add %r7d, %r7d            ; encoding: [0x20,0xfc]

# All 8 registers with qword
add %r0q, %r0q
# CHECK: add %r0q, %r0q            ; encoding: [0x30,0x00]
add %r1q, %r1q
# CHECK: add %r1q, %r1q            ; encoding: [0x30,0x24]
add %r2q, %r2q
# CHECK: add %r2q, %r2q            ; encoding: [0x30,0x48]
add %r3q, %r3q
# CHECK: add %r3q, %r3q            ; encoding: [0x30,0x6c]
add %r4q, %r4q
# CHECK: add %r4q, %r4q            ; encoding: [0x30,0x90]
add %r5q, %r5q
# CHECK: add %r5q, %r5q            ; encoding: [0x30,0xb4]
add %r6q, %r6q
# CHECK: add %r6q, %r6q            ; encoding: [0x30,0xd8]
add %r7q, %r7q
# CHECK: add %r7q, %r7q            ; encoding: [0x30,0xfc]

# Dword immediate edge cases
add %r0d, 0
# CHECK: add %r0d, 0               ; encoding: [0x60,0x00]
add %r0d, -16
# CHECK: add %r0d, -16             ; encoding: [0x60,0x10]
add %r0d, 15
# CHECK: add %r0d, 15              ; encoding: [0x60,0x0f]
movz %r0d, 0
# CHECK: movz %r0d, 0              ; encoding: [0x68,0x00]
movz %r0d, 31
# CHECK: movz %r0d, 31             ; encoding: [0x68,0x1f]

# Qword immediate edge cases
add %r0q, 0
# CHECK: add %r0q, 0               ; encoding: [0x70,0x00]
add %r0q, -16
# CHECK: add %r0q, -16             ; encoding: [0x70,0x10]
add %r0q, 15
# CHECK: add %r0q, 15              ; encoding: [0x70,0x0f]
movz %r0q, 0
# CHECK: movz %r0q, 0              ; encoding: [0x78,0x00]
movz %r0q, 31
# CHECK: movz %r0q, 31             ; encoding: [0x78,0x1f]

# Dword store to all 8 source registers (store to r1)
store %r0d, %r1d
# CHECK: store %r0d, %r1d          ; encoding: [0x2b,0x04]
store %r1d, %r1d
# CHECK: store %r1d, %r1d          ; encoding: [0x2b,0x24]
store %r2d, %r1d
# CHECK: store %r2d, %r1d          ; encoding: [0x2b,0x44]
store %r3d, %r1d
# CHECK: store %r3d, %r1d          ; encoding: [0x2b,0x64]
store %r4d, %r1d
# CHECK: store %r4d, %r1d          ; encoding: [0x2b,0x84]
store %r5d, %r1d
# CHECK: store %r5d, %r1d          ; encoding: [0x2b,0xa4]
store %r6d, %r1d
# CHECK: store %r6d, %r1d          ; encoding: [0x2b,0xc4]
store %r7d, %r1d
# CHECK: store %r7d, %r1d          ; encoding: [0x2b,0xe4]

# Qword load from all 8 address registers
load %r0q, %r0q
# CHECK: load %r0q, %r0q           ; encoding: [0x3a,0x00]
load %r0q, %r1q
# CHECK: load %r0q, %r1q           ; encoding: [0x3a,0x04]
load %r0q, %r2q
# CHECK: load %r0q, %r2q           ; encoding: [0x3a,0x08]
load %r0q, %r3q
# CHECK: load %r0q, %r3q           ; encoding: [0x3a,0x0c]
load %r0q, %r4q
# CHECK: load %r0q, %r4q           ; encoding: [0x3a,0x10]
load %r0q, %r5q
# CHECK: load %r0q, %r5q           ; encoding: [0x3a,0x14]
load %r0q, %r6q
# CHECK: load %r0q, %r6q           ; encoding: [0x3a,0x18]
load %r0q, %r7q
# CHECK: load %r0q, %r7q           ; encoding: [0x3a,0x1c]
