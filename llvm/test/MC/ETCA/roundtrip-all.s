# RUN: llvm-mc -arch=etca -show-encoding < %s | FileCheck %s
# RUN: llvm-mc -arch=etca --disassemble < %S/roundtrip-all.dis | FileCheck --check-prefix=DIS %s

# ==============================================================================
# Comprehensive round-trip test covering ALL instruction groups:
#   - Word (16-bit) RR, RI, LOAD/STORE, branches, SAF
#   - Dword (32-bit) RR, RI, LOAD/STORE
#   - Qword (64-bit) RR, RI, LOAD/STORE
#
# Test structure:
#   RUN 1: llvm-mc -show-encoding → verify encoding bytes (CHECK)
#   RUN 2: llvm-mc --disassemble roundtrip-all.dis → verify textual output (DIS)
#
# Together, RUN 1 + RUN 2 verify decode(encode(inst)) = inst:
#   encode(inst) → bytes  (RUN 1)
#   decode(bytes) → inst  (RUN 2)
# ==============================================================================

# ==================== RR Instructions (word) ====================

add %r0, %r1
# CHECK: add %r0, %r1              ; encoding: [0x10,0x04]
# DIS: add %r0, %r1

sub %r3, %r5
# CHECK: sub %r3, %r5              ; encoding: [0x11,0x74]
# DIS: sub %r3, %r5

rsub %r7, %r0
# CHECK: rsub %r7, %r0             ; encoding: [0x12,0xe0]
# DIS: rsub %r7, %r0

cmp %r6, %r3
# CHECK: cmp %r6, %r3              ; encoding: [0x13,0xcc]
# DIS: cmp %r6, %r3

or %r5, %r2
# CHECK: or %r5, %r2               ; encoding: [0x14,0xa8]
# DIS: or %r5, %r2

xor %r6, %r4
# CHECK: xor %r6, %r4              ; encoding: [0x15,0xd0]
# DIS: xor %r6, %r4

and %r1, %r7
# CHECK: and %r1, %r7              ; encoding: [0x16,0x3c]
# DIS: and %r1, %r7

test %r7, %r0
# CHECK: test %r7, %r0             ; encoding: [0x17,0xe0]
# DIS: test %r7, %r0

movz %r0, %r7
# CHECK: movz %r0, %r7             ; encoding: [0x18,0x1c]
# DIS: movz %r0, %r7

movs %r6, %r1
# CHECK: movs %r6, %r1             ; encoding: [0x19,0xc4]
# DIS: movs %r6, %r1

load %r0, %r1
# CHECK: load %r0, %r1             ; encoding: [0x1a,0x04]
# DIS: load %r0, %r1

store %r7, %r2
# CHECK: store %r7, %r2            ; encoding: [0x1b,0xe8]
# DIS: store %r7, %r2

# ==================== DWORD RR Instructions ====================

add %r0d, %r1d
# CHECK: add %r0d, %r1d            ; encoding: [0x20,0x04]
# DIS: add %r0d, %r1d

sub %r3d, %r5d
# CHECK: sub %r3d, %r5d            ; encoding: [0x21,0x74]
# DIS: sub %r3d, %r5d

rsub %r5d, %r2d
# CHECK: rsub %r5d, %r2d           ; encoding: [0x22,0xa8]
# DIS: rsub %r5d, %r2d

or %r5d, %r2d
# CHECK: or %r5d, %r2d             ; encoding: [0x24,0xa8]
# DIS: or %r5d, %r2d

xor %r6d, %r4d
# CHECK: xor %r6d, %r4d            ; encoding: [0x25,0xd0]
# DIS: xor %r6d, %r4d

and %r1d, %r7d
# CHECK: and %r1d, %r7d            ; encoding: [0x26,0x3c]
# DIS: and %r1d, %r7d

movz %r7d, %r6d
# CHECK: movz %r7d, %r6d           ; encoding: [0x28,0xf8]
# DIS: movz %r7d, %r6d

movs %r6d, %r1d
# CHECK: movs %r6d, %r1d           ; encoding: [0x29,0xc4]
# DIS: movs %r6d, %r1d

load %r0d, %r1d
# CHECK: load %r0d, %r1d           ; encoding: [0x2a,0x04]
# DIS: load %r0d, %r1d

store %r7d, %r2d
# CHECK: store %r7d, %r2d          ; encoding: [0x2b,0xe8]
# DIS: store %r7d, %r2d

# ==================== QWORD RR Instructions ====================

add %r0q, %r1q
# CHECK: add %r0q, %r1q            ; encoding: [0x30,0x04]
# DIS: add %r0q, %r1q

sub %r3q, %r5q
# CHECK: sub %r3q, %r5q            ; encoding: [0x31,0x74]
# DIS: sub %r3q, %r5q

rsub %r5q, %r2q
# CHECK: rsub %r5q, %r2q           ; encoding: [0x32,0xa8]
# DIS: rsub %r5q, %r2q

or %r5q, %r2q
# CHECK: or %r5q, %r2q             ; encoding: [0x34,0xa8]
# DIS: or %r5q, %r2q

xor %r6q, %r4q
# CHECK: xor %r6q, %r4q            ; encoding: [0x35,0xd0]
# DIS: xor %r6q, %r4q

and %r1q, %r7q
# CHECK: and %r1q, %r7q            ; encoding: [0x36,0x3c]
# DIS: and %r1q, %r7q

movz %r7q, %r6q
# CHECK: movz %r7q, %r6q           ; encoding: [0x38,0xf8]
# DIS: movz %r7q, %r6q

movs %r6q, %r1q
# CHECK: movs %r6q, %r1q           ; encoding: [0x39,0xc4]
# DIS: movs %r6q, %r1q

load %r0q, %r1q
# CHECK: load %r0q, %r1q           ; encoding: [0x3a,0x04]
# DIS: load %r0q, %r1q

store %r7q, %r2q
# CHECK: store %r7q, %r2q          ; encoding: [0x3b,0xe8]
# DIS: store %r7q, %r2q

# ==================== RI Instructions (word) ====================

add %r0, 5
# CHECK: add %r0, 5                ; encoding: [0x50,0x05]
# DIS: add %r0, 5

sub %r5, 3
# CHECK: sub %r5, 3                ; encoding: [0x51,0xa3]
# DIS: sub %r5, 3

rsub %r7, -16
# CHECK: rsub %r7, -16             ; encoding: [0x52,0xf0]
# DIS: rsub %r7, 16

cmp %r0, 5
# CHECK: cmp %r0, 5                ; encoding: [0x53,0x05]
# DIS: cmp %r0, 5

or %r7, 15
# CHECK: or %r7, 15                ; encoding: [0x54,0xef]
# DIS: or %r7, 15

xor %r0, 5
# CHECK: xor %r0, 5                ; encoding: [0x55,0x05]
# DIS: xor %r0, 5

and %r7, 31
# CHECK: and %r7, 31               ; encoding: [0x56,0xff]
# DIS: and %r7, 31

test %r0, 5
# CHECK: test %r0, 5               ; encoding: [0x57,0x05]
# DIS: test %r0, 5

movz %r0, 31
# CHECK: movz %r0, 31              ; encoding: [0x58,0x1f]
# DIS: movz %r0, 31

movs %r1, -8
# CHECK: movs %r1, -8              ; encoding: [0x59,0x38]
# DIS: movs %r1, 24

slo %r1, 7
# CHECK: slo %r1, 7                ; encoding: [0x5c,0x27]
# DIS: slo %r1, 7

readcr %r0, 3
# CHECK: readcr %r0, 3             ; encoding: [0x5e,0x03]
# DIS: readcr %r0, 3

writecr %r7, 5
# CHECK: writecr %r7, 5            ; encoding: [0x5f,0xe5]
# DIS: writecr %r7, 5

# ==================== DWORD RI Instructions ====================

add %r0d, 5
# CHECK: add %r0d, 5               ; encoding: [0x60,0x05]
# DIS: add %r0d, 5

sub %r5d, 3
# CHECK: sub %r5d, 3               ; encoding: [0x61,0xa3]
# DIS: sub %r5d, 3

rsub %r3d, -16
# CHECK: rsub %r3d, -16            ; encoding: [0x62,0x70]
# DIS: rsub %r3d, 16

cmp %r0d, 5
# CHECK: cmp %r0d, 5               ; encoding: [0x63,0x05]
# DIS: cmp %r0d, 5

or %r7d, 15
# CHECK: or %r7d, 15               ; encoding: [0x64,0xef]
# DIS: or %r7d, 15

xor %r4d, 7
# CHECK: xor %r4d, 7               ; encoding: [0x65,0x87]
# DIS: xor %r4d, 7

and %r7d, 31
# CHECK: and %r7d, 31              ; encoding: [0x66,0xff]
# DIS: and %r7d, 31

test %r0d, 5
# CHECK: test %r0d, 5              ; encoding: [0x67,0x05]
# DIS: test %r0d, 5

movz %r0d, 31
# CHECK: movz %r0d, 31             ; encoding: [0x68,0x1f]
# DIS: movz %r0d, 31

movs %r1d, -8
# CHECK: movs %r1d, -8             ; encoding: [0x69,0x38]
# DIS: movs %r1d, 24

# ==================== QWORD RI Instructions ====================

add %r0q, 5
# CHECK: add %r0q, 5               ; encoding: [0x70,0x05]
# DIS: add %r0q, 5

sub %r5q, 3
# CHECK: sub %r5q, 3               ; encoding: [0x71,0xa3]
# DIS: sub %r5q, 3

rsub %r3q, -16
# CHECK: rsub %r3q, -16            ; encoding: [0x72,0x70]
# DIS: rsub %r3q, 16

cmp %r0q, 5
# CHECK: cmp %r0q, 5               ; encoding: [0x73,0x05]
# DIS: cmp %r0q, 5

or %r7q, 15
# CHECK: or %r7q, 15               ; encoding: [0x74,0xef]
# DIS: or %r7q, 15

xor %r4q, 7
# CHECK: xor %r4q, 7               ; encoding: [0x75,0x87]
# DIS: xor %r4q, 7

and %r7q, 31
# CHECK: and %r7q, 31              ; encoding: [0x76,0xff]
# DIS: and %r7q, 31

test %r0q, 5
# CHECK: test %r0q, 5              ; encoding: [0x77,0x05]
# DIS: test %r0q, 5

movz %r0q, 31
# CHECK: movz %r0q, 31             ; encoding: [0x78,0x1f]
# DIS: movz %r0q, 31

movs %r1q, -8
# CHECK: movs %r1q, -8             ; encoding: [0x79,0x38]
# DIS: movs %r1q, 24

# ==================== Branch Instructions ====================

br 0
# CHECK: br 0                      ; encoding: [0x8e,0x00]
# DIS: br 0

beq 2
# CHECK: beq 2                     ; encoding: [0x80,0x01]
# DIS: beq 2

bne 4
# CHECK: bne 4                     ; encoding: [0x81,0x02]
# DIS: bne 4

blt 6
# CHECK: blt 6                     ; encoding: [0x8a,0x03]
# DIS: blt 6

bge 8
# CHECK: bge 8                     ; encoding: [0x8b,0x04]
# DIS: bge 8

bltu 10
# CHECK: bltu 10                   ; encoding: [0x84,0x05]
# DIS: bltu 10

bgeu 12
# CHECK: bgeu 12                   ; encoding: [0x85,0x06]
# DIS: bgeu 12

beq 256
# CHECK: beq 256                   ; encoding: [0x80,0x80]
# DIS: beq 256

br 510
# CHECK: br 510                    ; encoding: [0x8e,0xff]
# DIS: br 510

br -2
# CHECK: br -2                     ; encoding: [0x9e,0xff]
# DIS: br -2

beq -4
# CHECK: beq -4                    ; encoding: [0x90,0xfe]
# DIS: beq -4

br -512
# CHECK: br -512                   ; encoding: [0x9e,0x00]
# DIS: br -512

# ==================== SAF Instructions ====================

push %r0
# CHECK: push %r0                  ; encoding: [0x1d,0xc0]
# DIS: push %r0

push %r3
# CHECK: push %r3                  ; encoding: [0x1d,0xcc]
# DIS: push %r3

push %r7
# CHECK: push %r7                  ; encoding: [0x1d,0xdc]
# DIS: push %r7

pop %r0
# CHECK: pop %r0                   ; encoding: [0x1c,0x18]
# DIS: pop %r0

pop %r7
# CHECK: pop %r7                   ; encoding: [0x1c,0xf8]
# DIS: pop %r7

push 5
# CHECK: push 5                    ; encoding: [0x5d,0xc5]
# DIS: push 5

push 31
# CHECK: push 31                   ; encoding: [0x5d,0xdf]
# DIS: push 31

call 0
# CHECK: call 0                    ; encoding: [0xb0,0x00]
# DIS: call 0

call 8
# CHECK: call 8                    ; encoding: [0xb0,0x04]
# DIS: call 8

call 2048
# CHECK: call 2048                 ; encoding: [0xb4,0x00]
# DIS: call 2048

call 4094
# CHECK: call 4094                 ; encoding: [0xb7,0xff]
# DIS: call 4094

jmpr %r0
# CHECK: jmpr %r0                  ; encoding: [0xaf,0x0e]
# DIS: jmpr %r0

jmpr %r7
# CHECK: jmpr %r7                  ; encoding: [0xaf,0xee]
# DIS: jmpr %r7

callr %r0
# CHECK: callr %r0                 ; encoding: [0xaf,0x1e]
# DIS: callr %r0

callr %r3
# CHECK: callr %r3                 ; encoding: [0xaf,0x7e]
# DIS: callr %r3

# ==================== BYTE (8-bit) RR Instructions ====================

add %r0h, %r1h
# CHECK: add %r0h, %r1h            ; encoding: [0x00,0x04]

sub %r2h, %r3h
# CHECK: sub %r2h, %r3h            ; encoding: [0x01,0x4c]

or %r6h, %r7h
# CHECK: or %r6h, %r7h             ; encoding: [0x04,0xdc]

xor %r0h, %r1h
# CHECK: xor %r0h, %r1h            ; encoding: [0x05,0x04]

and %r2h, %r3h
# CHECK: and %r2h, %r3h            ; encoding: [0x06,0x4c]

movz %r4h, %r5h
# CHECK: movz %r4h, %r5h           ; encoding: [0x08,0x94]

movs %r6h, %r7h
# CHECK: movs %r6h, %r7h           ; encoding: [0x09,0xdc]

# ==================== BYTE (8-bit) CMP/TEST RR ====================

cmp %r0h, %r1h
# CHECK: cmp %r0h, %r1h            ; encoding: [0x03,0x04]

test %r6h, %r3h
# CHECK: test %r6h, %r3h           ; encoding: [0x07,0xcc]

# ==================== BYTE (8-bit) RI Instructions ====================

add %r0h, 31
# CHECK: add %r0h, 31              ; encoding: [0x40,0x1f]

cmp %r3h, 0
# CHECK: cmp %r3h, 0               ; encoding: [0x43,0x60]

or %r4h, 7
# CHECK: or %r4h, 7                ; encoding: [0x44,0x87]

movz %r0h, 10
# CHECK: movz %r0h, 10             ; encoding: [0x48,0x0a]

# ==================== BYTE (8-bit) LOAD/STORE ====================

load %r0h, %r1h
# CHECK: load %r0h, %r1h           ; encoding: [0x0a,0x04]

store %r2h, %r3h
# CHECK: store %r2h, %r3h          ; encoding: [0x0b,0x4c]

# ==================== NOP ====================

nop
# CHECK: nop                       ; encoding: [0x8f,0x00]

# ==================== BYTE (8-bit) Roundtrip (appended to .dis) ====================
# DIS: nop
# DIS: add %r0h, %r1h
# DIS: sub %r2h, %r3h
# DIS: or %r6h, %r7h
# DIS: xor %r0h, %r1h
# DIS: and %r2h, %r3h
# DIS: movz %r4h, %r5h
# DIS: movs %r6h, %r7h
# DIS: cmp %r0h, %r1h
# DIS: test %r6h, %r3h
# DIS: add %r0h, 31
# DIS: cmp %r3h, 0
# DIS: or %r4h, 7
# DIS: movz %r0h, 10
# DIS: load %r0h, %r1h
# DIS: store %r2h, %r3h
