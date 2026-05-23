# RUN: llvm-mc -arch=etca --disassemble < %s | llvm-mc -arch=etca --show-encoding | FileCheck %s
# RUN: llvm-mc -arch=etca --disassemble < %s | FileCheck --check-prefix=DIS %s

# ==============================================================================
# Comprehensive disassembler test covering ALL instruction formats.
# Each line is a raw instruction encoding (hex bytes, little-endian).
#
# RUN 1: disassemble bytes → text → re-assemble → verify encoding matches
# RUN 2: disassemble bytes → text, verify instruction mnemonic and operands
# ==============================================================================

# ==================== Byte (8-bit) RR Instructions ====================

# add %r0h, %r1h → [0x00,0x04]
0x00 0x04
# CHECK: add %r0h, %r1h            ; encoding: [0x00,0x04]
# DIS: add %r0h, %r1h

# sub %r2h, %r3h → [0x01,0x4c]
0x01 0x4c
# CHECK: sub %r2h, %r3h            ; encoding: [0x01,0x4c]
# DIS: sub %r2h, %r3h

# rsub %r0h, %r1h → [0x02,0x04]
0x02 0x04
# CHECK: rsub %r0h, %r1h           ; encoding: [0x02,0x04]
# DIS: rsub %r0h, %r1h

# cmp %r0h, %r1h → [0x03,0x04]
0x03 0x04
# CHECK: cmp %r0h, %r1h            ; encoding: [0x03,0x04]
# DIS: cmp %r0h, %r1h

# or %r6h, %r7h → [0x04,0xdc]
0x04 0xdc
# CHECK: or %r6h, %r7h             ; encoding: [0x04,0xdc]
# DIS: or %r6h, %r7h

# xor %r0h, %r1h → [0x05,0x04]
0x05 0x04
# CHECK: xor %r0h, %r1h            ; encoding: [0x05,0x04]
# DIS: xor %r0h, %r1h

# and %r2h, %r3h → [0x06,0x4c]
0x06 0x4c
# CHECK: and %r2h, %r3h            ; encoding: [0x06,0x4c]
# DIS: and %r2h, %r3h

# test %r6h, %r3h → [0x07,0xcc]
0x07 0xcc
# CHECK: test %r6h, %r3h           ; encoding: [0x07,0xcc]
# DIS: test %r6h, %r3h

# movz %r4h, %r5h → [0x08,0x94]
0x08 0x94
# CHECK: movz %r4h, %r5h           ; encoding: [0x08,0x94]
# DIS: movz %r4h, %r5h

# movs %r6h, %r7h → [0x09,0xdc]
0x09 0xdc
# CHECK: movs %r6h, %r7h           ; encoding: [0x09,0xdc]
# DIS: movs %r6h, %r7h

# load %r0h, %r1h → [0x0a,0x04]
0x0a 0x04
# CHECK: load %r0h, %r1h           ; encoding: [0x0a,0x04]
# DIS: load %r0h, %r1h

# store %r2h, %r3h → [0x0b,0x4c]
0x0b 0x4c
# CHECK: store %r2h, %r3h          ; encoding: [0x0b,0x4c]
# DIS: store %r2h, %r3h

# ==================== Byte (8-bit) RI Instructions ====================

# add %r0h, 31 → [0x40,0x1f]
0x40 0x1f
# CHECK: add %r0h, 31              ; encoding: [0x40,0x1f]
# DIS: add %r0h, 31

# sub %r0h, 15 → [0x41,0x0f]
0x41 0x0f
# CHECK: sub %r0h, 15              ; encoding: [0x41,0x0f]
# DIS: sub %r0h, 15

# cmp %r3h, 0 → [0x43,0x60]
0x43 0x60
# CHECK: cmp %r3h, 0               ; encoding: [0x43,0x60]
# DIS: cmp %r3h, 0

# or %r4h, 7 → [0x44,0x87]
0x44 0x87
# CHECK: or %r4h, 7                ; encoding: [0x44,0x87]
# DIS: or %r4h, 7

# movz %r0h, 10 → [0x48,0x0a]
0x48 0x0a
# CHECK: movz %r0h, 10             ; encoding: [0x48,0x0a]
# DIS: movz %r0h, 10

# ==================== Word (16-bit) RR Instructions ====================

# add %r0, %r1 → [0x10,0x04]
0x10 0x04
# CHECK: add %r0, %r1              ; encoding: [0x10,0x04]
# DIS: add %r0, %r1

# sub %r3, %r5 → [0x11,0x74]
0x11 0x74
# CHECK: sub %r3, %r5              ; encoding: [0x11,0x74]
# DIS: sub %r3, %r5

# rsub %r7, %r0 → [0x12,0xe0]
0x12 0xe0
# CHECK: rsub %r7, %r0             ; encoding: [0x12,0xe0]
# DIS: rsub %r7, %r0

# cmp %r6, %r3 → [0x13,0xcc]
0x13 0xcc
# CHECK: cmp %r6, %r3              ; encoding: [0x13,0xcc]
# DIS: cmp %r6, %r3

# or %r5, %r2 → [0x14,0xa8]
0x14 0xa8
# CHECK: or %r5, %r2               ; encoding: [0x14,0xa8]
# DIS: or %r5, %r2

# xor %r6, %r4 → [0x15,0xd0]
0x15 0xd0
# CHECK: xor %r6, %r4              ; encoding: [0x15,0xd0]
# DIS: xor %r6, %r4

# and %r1, %r7 → [0x16,0x3c]
0x16 0x3c
# CHECK: and %r1, %r7              ; encoding: [0x16,0x3c]
# DIS: and %r1, %r7

# test %r7, %r0 → [0x17,0xe0]
0x17 0xe0
# CHECK: test %r7, %r0             ; encoding: [0x17,0xe0]
# DIS: test %r7, %r0

# movz %r0, %r7 → [0x18,0x1c]
0x18 0x1c
# CHECK: movz %r0, %r7             ; encoding: [0x18,0x1c]
# DIS: movz %r0, %r7

# movs %r6, %r1 → [0x19,0xc4]
0x19 0xc4
# CHECK: movs %r6, %r1             ; encoding: [0x19,0xc4]
# DIS: movs %r6, %r1

# load %r0, %r1 → [0x1a,0x04]
0x1a 0x04
# CHECK: load %r0, %r1             ; encoding: [0x1a,0x04]
# DIS: load %r0, %r1

# store %r7, %r2 → [0x1b,0xe8]
0x1b 0xe8
# CHECK: store %r7, %r2            ; encoding: [0x1b,0xe8]
# DIS: store %r7, %r2

# ==================== Dword (32-bit) RR Instructions ====================

# add %r0d, %r1d → [0x20,0x04]
0x20 0x04
# CHECK: add %r0d, %r1d            ; encoding: [0x20,0x04]
# DIS: add %r0d, %r1d

# sub %r3d, %r5d → [0x21,0x74]
0x21 0x74
# CHECK: sub %r3d, %r5d            ; encoding: [0x21,0x74]
# DIS: sub %r3d, %r5d

# rsub %r5d, %r2d → [0x22,0xa8]
0x22 0xa8
# CHECK: rsub %r5d, %r2d           ; encoding: [0x22,0xa8]
# DIS: rsub %r5d, %r2d

# cmp %r6d, %r3d → [0x23,0xcc]
0x23 0xcc
# CHECK: cmp %r6d, %r3d            ; encoding: [0x23,0xcc]
# DIS: cmp %r6d, %r3d

# or %r5d, %r2d → [0x24,0xa8]
0x24 0xa8
# CHECK: or %r5d, %r2d             ; encoding: [0x24,0xa8]
# DIS: or %r5d, %r2d

# xor %r6d, %r4d → [0x25,0xd0]
0x25 0xd0
# CHECK: xor %r6d, %r4d            ; encoding: [0x25,0xd0]
# DIS: xor %r6d, %r4d

# and %r1d, %r7d → [0x26,0x3c]
0x26 0x3c
# CHECK: and %r1d, %r7d            ; encoding: [0x26,0x3c]
# DIS: and %r1d, %r7d

# test %r6d, %r3d → [0x27,0xcc]
0x27 0xcc
# CHECK: test %r6d, %r3d           ; encoding: [0x27,0xcc]
# DIS: test %r6d, %r3d

# movz %r7d, %r6d → [0x28,0xf8]
0x28 0xf8
# CHECK: movz %r7d, %r6d           ; encoding: [0x28,0xf8]
# DIS: movz %r7d, %r6d

# movs %r6d, %r1d → [0x29,0xc4]
0x29 0xc4
# CHECK: movs %r6d, %r1d           ; encoding: [0x29,0xc4]
# DIS: movs %r6d, %r1d

# load %r0d, %r1d → [0x2a,0x04]
0x2a 0x04
# CHECK: load %r0d, %r1d           ; encoding: [0x2a,0x04]
# DIS: load %r0d, %r1d

# store %r7d, %r2d → [0x2b,0xe8]
0x2b 0xe8
# CHECK: store %r7d, %r2d          ; encoding: [0x2b,0xe8]
# DIS: store %r7d, %r2d

# ==================== Qword (64-bit) RR Instructions ====================

# add %r0q, %r1q → [0x30,0x04]
0x30 0x04
# CHECK: add %r0q, %r1q            ; encoding: [0x30,0x04]
# DIS: add %r0q, %r1q

# sub %r3q, %r5q → [0x31,0x74]
0x31 0x74
# CHECK: sub %r3q, %r5q            ; encoding: [0x31,0x74]
# DIS: sub %r3q, %r5q

# rsub %r5q, %r2q → [0x32,0xa8]
0x32 0xa8
# CHECK: rsub %r5q, %r2q           ; encoding: [0x32,0xa8]
# DIS: rsub %r5q, %r2q

# cmp %r6q, %r3q → [0x33,0xcc]
0x33 0xcc
# CHECK: cmp %r6q, %r3q            ; encoding: [0x33,0xcc]
# DIS: cmp %r6q, %r3q

# or %r5q, %r2q → [0x34,0xa8]
0x34 0xa8
# CHECK: or %r5q, %r2q             ; encoding: [0x34,0xa8]
# DIS: or %r5q, %r2q

# xor %r6q, %r4q → [0x35,0xd0]
0x35 0xd0
# CHECK: xor %r6q, %r4q            ; encoding: [0x35,0xd0]
# DIS: xor %r6q, %r4q

# and %r1q, %r7q → [0x36,0x3c]
0x36 0x3c
# CHECK: and %r1q, %r7q            ; encoding: [0x36,0x3c]
# DIS: and %r1q, %r7q

# test %r6q, %r3q → [0x37,0xcc]
0x37 0xcc
# CHECK: test %r6q, %r3q           ; encoding: [0x37,0xcc]
# DIS: test %r6q, %r3q

# movz %r7q, %r6q → [0x38,0xf8]
0x38 0xf8
# CHECK: movz %r7q, %r6q           ; encoding: [0x38,0xf8]
# DIS: movz %r7q, %r6q

# movs %r6q, %r1q → [0x39,0xc4]
0x39 0xc4
# CHECK: movs %r6q, %r1q           ; encoding: [0x39,0xc4]
# DIS: movs %r6q, %r1q

# load %r0q, %r1q → [0x3a,0x04]
0x3a 0x04
# CHECK: load %r0q, %r1q           ; encoding: [0x3a,0x04]
# DIS: load %r0q, %r1q

# store %r7q, %r2q → [0x3b,0xe8]
0x3b 0xe8
# CHECK: store %r7q, %r2q          ; encoding: [0x3b,0xe8]
# DIS: store %r7q, %r2q

# ==================== Word (16-bit) RI Instructions ====================

# add %r0, 5 → [0x50,0x05]
0x50 0x05
# CHECK: add %r0, 5                ; encoding: [0x50,0x05]
# DIS: add %r0, 5

# sub %r5, 3 → [0x51,0xa3]
0x51 0xa3
# CHECK: sub %r5, 3                ; encoding: [0x51,0xa3]
# DIS: sub %r5, 3

# rsub %r7, 16 → [0x52,0xf0]
0x52 0xf0
# CHECK: rsub %r7, 16              ; encoding: [0x52,0xf0]
# DIS: rsub %r7, 16

# cmp %r0, 5 → [0x53,0x05]
0x53 0x05
# CHECK: cmp %r0, 5                ; encoding: [0x53,0x05]
# DIS: cmp %r0, 5

# or %r7, 15 → [0x54,0xef]
0x54 0xef
# CHECK: or %r7, 15                ; encoding: [0x54,0xef]
# DIS: or %r7, 15

# xor %r0, 5 → [0x55,0x05]
0x55 0x05
# CHECK: xor %r0, 5                ; encoding: [0x55,0x05]
# DIS: xor %r0, 5

# and %r7, 31 → [0x56,0xff]
0x56 0xff
# CHECK: and %r7, 31               ; encoding: [0x56,0xff]
# DIS: and %r7, 31

# test %r0, 5 → [0x57,0x05]
0x57 0x05
# CHECK: test %r0, 5               ; encoding: [0x57,0x05]
# DIS: test %r0, 5

# movz %r0, 31 → [0x58,0x1f]
0x58 0x1f
# CHECK: movz %r0, 31              ; encoding: [0x58,0x1f]
# DIS: movz %r0, 31

# movs %r1, 24 → [0x59,0x38]
0x59 0x38
# CHECK: movs %r1, 24              ; encoding: [0x59,0x38]
# DIS: movs %r1, 24

# slo %r1, 7 → [0x5c,0x27]
0x5c 0x27
# CHECK: slo %r1, 7                ; encoding: [0x5c,0x27]
# DIS: slo %r1, 7

# readcr %r0, 3 → [0x5e,0x03]
0x5e 0x03
# CHECK: readcr %r0, 3             ; encoding: [0x5e,0x03]
# DIS: readcr %r0, 3

# writecr %r7, 5 → [0x5f,0xe5]
0x5f 0xe5
# CHECK: writecr %r7, 5            ; encoding: [0x5f,0xe5]
# DIS: writecr %r7, 5

# ==================== Dword (32-bit) RI Instructions ====================

# add %r0d, 5 → [0x60,0x05]
0x60 0x05
# CHECK: add %r0d, 5               ; encoding: [0x60,0x05]
# DIS: add %r0d, 5

# sub %r5d, 3 → [0x61,0xa3]
0x61 0xa3
# CHECK: sub %r5d, 3               ; encoding: [0x61,0xa3]
# DIS: sub %r5d, 3

# rsub %r3d, 16 → [0x62,0x70]
0x62 0x70
# CHECK: rsub %r3d, 16             ; encoding: [0x62,0x70]
# DIS: rsub %r3d, 16

# cmp %r0d, 5 → [0x63,0x05]
0x63 0x05
# CHECK: cmp %r0d, 5               ; encoding: [0x63,0x05]
# DIS: cmp %r0d, 5

# or %r7d, 15 → [0x64,0xef]
0x64 0xef
# CHECK: or %r7d, 15               ; encoding: [0x64,0xef]
# DIS: or %r7d, 15

# xor %r4d, 7 → [0x65,0x87]
0x65 0x87
# CHECK: xor %r4d, 7               ; encoding: [0x65,0x87]
# DIS: xor %r4d, 7

# and %r7d, 31 → [0x66,0xff]
0x66 0xff
# CHECK: and %r7d, 31              ; encoding: [0x66,0xff]
# DIS: and %r7d, 31

# test %r0d, 5 → [0x67,0x05]
0x67 0x05
# CHECK: test %r0d, 5              ; encoding: [0x67,0x05]
# DIS: test %r0d, 5

# movz %r0d, 31 → [0x68,0x1f]
0x68 0x1f
# CHECK: movz %r0d, 31             ; encoding: [0x68,0x1f]
# DIS: movz %r0d, 31

# movs %r1d, 24 → [0x69,0x38]
0x69 0x38
# CHECK: movs %r1d, 24             ; encoding: [0x69,0x38]
# DIS: movs %r1d, 24

# ==================== Qword (64-bit) RI Instructions ====================

# add %r0q, 5 → [0x70,0x05]
0x70 0x05
# CHECK: add %r0q, 5               ; encoding: [0x70,0x05]
# DIS: add %r0q, 5

# sub %r5q, 3 → [0x71,0xa3]
0x71 0xa3
# CHECK: sub %r5q, 3               ; encoding: [0x71,0xa3]
# DIS: sub %r5q, 3

# rsub %r3q, 16 → [0x72,0x70]
0x72 0x70
# CHECK: rsub %r3q, 16             ; encoding: [0x72,0x70]
# DIS: rsub %r3q, 16

# cmp %r0q, 5 → [0x73,0x05]
0x73 0x05
# CHECK: cmp %r0q, 5               ; encoding: [0x73,0x05]
# DIS: cmp %r0q, 5

# or %r7q, 15 → [0x74,0xef]
0x74 0xef
# CHECK: or %r7q, 15               ; encoding: [0x74,0xef]
# DIS: or %r7q, 15

# xor %r4q, 7 → [0x75,0x87]
0x75 0x87
# CHECK: xor %r4q, 7               ; encoding: [0x75,0x87]
# DIS: xor %r4q, 7

# and %r7q, 31 → [0x76,0xff]
0x76 0xff
# CHECK: and %r7q, 31              ; encoding: [0x76,0xff]
# DIS: and %r7q, 31

# test %r0q, 5 → [0x77,0x05]
0x77 0x05
# CHECK: test %r0q, 5              ; encoding: [0x77,0x05]
# DIS: test %r0q, 5

# movz %r0q, 31 → [0x78,0x1f]
0x78 0x1f
# CHECK: movz %r0q, 31             ; encoding: [0x78,0x1f]
# DIS: movz %r0q, 31

# movs %r1q, 24 → [0x79,0x38]
0x79 0x38
# CHECK: movs %r1q, 24             ; encoding: [0x79,0x38]
# DIS: movs %r1q, 24

# ==================== Branch Instructions ====================

# br 0 → [0x8e,0x00]
0x8e 0x00
# CHECK: br 0                      ; encoding: [0x8e,0x00]
# DIS: br 0

# beq 2 → [0x80,0x01]
0x80 0x01
# CHECK: beq 2                     ; encoding: [0x80,0x01]
# DIS: beq 2

# bne 4 → [0x81,0x02]
0x81 0x02
# CHECK: bne 4                     ; encoding: [0x81,0x02]
# DIS: bne 4

# bn 6 → [0x82,0x03]
0x82 0x03
# CHECK: bn 6                      ; encoding: [0x82,0x03]
# DIS: bn 6

# bnn 8 → [0x83,0x04]
0x83 0x04
# CHECK: bnn 8                     ; encoding: [0x83,0x04]
# DIS: bnn 8

# bltu 10 → [0x84,0x05]
0x84 0x05
# CHECK: bltu 10                   ; encoding: [0x84,0x05]
# DIS: bltu 10

# bgeu 12 → [0x85,0x06]
0x85 0x06
# CHECK: bgeu 12                   ; encoding: [0x85,0x06]
# DIS: bgeu 12

# bov 14 → [0x86,0x07]
0x86 0x07
# CHECK: bov 14                    ; encoding: [0x86,0x07]
# DIS: bov 14

# bnov 16 → [0x87,0x08]
0x87 0x08
# CHECK: bnov 16                   ; encoding: [0x87,0x08]
# DIS: bnov 16

# bleu 18 → [0x88,0x09]
0x88 0x09
# CHECK: bleu 18                   ; encoding: [0x88,0x09]
# DIS: bleu 18

# bgtu 20 → [0x89,0x0a]
0x89 0x0a
# CHECK: bgtu 20                   ; encoding: [0x89,0x0a]
# DIS: bgtu 20

# blt 22 → [0x8a,0x0b]
0x8a 0x0b
# CHECK: blt 22                    ; encoding: [0x8a,0x0b]
# DIS: blt 22

# bge 24 → [0x8b,0x0c]
0x8b 0x0c
# CHECK: bge 24                    ; encoding: [0x8b,0x0c]
# DIS: bge 24

# ble 26 → [0x8c,0x0d]
0x8c 0x0d
# CHECK: ble 26                    ; encoding: [0x8c,0x0d]
# DIS: ble 26

# bgt 28 → [0x8d,0x0e]
0x8d 0x0e
# CHECK: bgt 28                    ; encoding: [0x8d,0x0e]
# DIS: bgt 28

# ==================== Branch Edge Cases ====================

# beq 256 (maximum positive displacement, D8=1) → [0x80,0x80]
0x80 0x80
# CHECK: beq 256                   ; encoding: [0x80,0x80]
# DIS: beq 256

# br 510 (max positive: 255*2) → [0x8e,0xff]
0x8e 0xff
# CHECK: br 510                    ; encoding: [0x8e,0xff]
# DIS: br 510

# br -2 (small negative) → [0x9e,0xff]
0x9e 0xff
# CHECK: br -2                     ; encoding: [0x9e,0xff]
# DIS: br -2

# beq -4 → [0x90,0xfe]
0x90 0xfe
# CHECK: beq -4                    ; encoding: [0x90,0xfe]
# DIS: beq -4

# br -512 (max negative) → [0x9e,0x00]
0x9e 0x00
# CHECK: br -512                   ; encoding: [0x9e,0x00]
# DIS: br -512

# ==================== SAF Stack Instructions ====================

# push %r0 → [0x1d,0xc0]
0x1d 0xc0
# CHECK: push %r0                  ; encoding: [0x1d,0xc0]
# DIS: push %r0

# push %r3 → [0x1d,0xcc]
0x1d 0xcc
# CHECK: push %r3                  ; encoding: [0x1d,0xcc]
# DIS: push %r3

# push %r7 → [0x1d,0xdc]
0x1d 0xdc
# CHECK: push %r7                  ; encoding: [0x1d,0xdc]
# DIS: push %r7

# push %r3d → [0x2d,0xcc]
0x2d 0xcc
# CHECK: push %r3d                 ; encoding: [0x2d,0xcc]
# DIS: push %r3d

# push %r3q → [0x3d,0xcc]
0x3d 0xcc
# CHECK: push %r3q                 ; encoding: [0x3d,0xcc]
# DIS: push %r3q

# pop %r0 → [0x1c,0x18]
0x1c 0x18
# CHECK: pop %r0                   ; encoding: [0x1c,0x18]
# DIS: pop %r0

# pop %r7 → [0x1c,0xf8]
0x1c 0xf8
# CHECK: pop %r7                   ; encoding: [0x1c,0xf8]
# DIS: pop %r7

# pop %r0d → [0x2c,0x18]
0x2c 0x18
# CHECK: pop %r0d                  ; encoding: [0x2c,0x18]
# DIS: pop %r0d

# pop %r0q → [0x3c,0x18]
0x3c 0x18
# CHECK: pop %r0q                  ; encoding: [0x3c,0x18]
# DIS: pop %r0q

# push 5 → [0x5d,0xc5]
0x5d 0xc5
# CHECK: push 5                    ; encoding: [0x5d,0xc5]
# DIS: push 5

# push 31 → [0x5d,0xdf]
0x5d 0xdf
# CHECK: push 31                   ; encoding: [0x5d,0xdf]
# DIS: push 31

# ==================== SAF CALL ====================

# call 0 → [0xb0,0x00]
0xb0 0x00
# CHECK: call 0                    ; encoding: [0xb0,0x00]
# DIS: call 0

# call 8 → [0xb0,0x04]
0xb0 0x04
# CHECK: call 8                    ; encoding: [0xb0,0x04]
# DIS: call 8

# call 2048 → [0xb4,0x00]
0xb4 0x00
# CHECK: call 2048                 ; encoding: [0xb4,0x00]
# DIS: call 2048

# call 4094 → [0xb7,0xff]
0xb7 0xff
# CHECK: call 4094                 ; encoding: [0xb7,0xff]
# DIS: call 4094

# call -2 → [0xbf,0xff]
0xbf 0xff
# CHECK: call -2                   ; encoding: [0xbf,0xff]
# DIS: call -2

# ==================== SAF JMPR/CALLR ====================

# jmpr %r0 → [0xaf,0x0e]
0xaf 0x0e
# CHECK: jmpr %r0                  ; encoding: [0xaf,0x0e]
# DIS: jmpr %r0

# jmpr %r7 → [0xaf,0xee]
0xaf 0xee
# CHECK: jmpr %r7                  ; encoding: [0xaf,0xee]
# DIS: jmpr %r7

# callr %r0 → [0xaf,0x1e]
0xaf 0x1e
# CHECK: callr %r0                 ; encoding: [0xaf,0x1e]
# DIS: callr %r0

# callr %r3 → [0xaf,0x7e]
0xaf 0x7e
# CHECK: callr %r3                 ; encoding: [0xaf,0x7e]
# DIS: callr %r3

# ==================== NOP ====================

# nop → [0x8f,0x00]
0x8f 0x00
# CHECK: nop                       ; encoding: [0x8f,0x00]
# DIS: nop


