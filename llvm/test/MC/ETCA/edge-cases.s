# RUN: llvm-mc -arch=etca -show-encoding < %s | FileCheck %s

# ==============================================================================
# Edge case tests for corner-case instruction encodings.
# Tests boundary values, all register combinations, and unusual operand combos.
# ==============================================================================

# ==================== Immediate Boundary Tests ====================

# Maximum 5-bit unsigned immediate (MOVZ, where imm is zero-extended)
movz %r0, 31
# CHECK: movz %r0, 31              ; encoding: [0x58,0x1f]
movz %r0d, 31
# CHECK: movz %r0d, 31             ; encoding: [0x68,0x1f]
movz %r0q, 31
# CHECK: movz %r0q, 31             ; encoding: [0x78,0x1f]

# Minimum 5-bit signed immediate (-16)
add %r0, -16
# CHECK: add %r0, -16              ; encoding: [0x50,0x10]
add %r0d, -16
# CHECK: add %r0d, -16             ; encoding: [0x60,0x10]
add %r0q, -16
# CHECK: add %r0q, -16             ; encoding: [0x70,0x10]

# Maximum 5-bit signed positive (15)
add %r0, 15
# CHECK: add %r0, 15               ; encoding: [0x50,0x0f]
add %r0d, 15
# CHECK: add %r0d, 15              ; encoding: [0x60,0x0f]
add %r0q, 15
# CHECK: add %r0q, 15              ; encoding: [0x70,0x0f]

# Zero immediate
add %r0, 0
# CHECK: add %r0, 0                ; encoding: [0x50,0x00]
add %r0d, 0
# CHECK: add %r0d, 0               ; encoding: [0x60,0x00]
add %r0q, 0
# CHECK: add %r0q, 0               ; encoding: [0x70,0x00]

# ==================== SLO tests ====================

slo %r0, 0
# CHECK: slo %r0, 0                ; encoding: [0x5c,0x00]
slo %r0, 31
# CHECK: slo %r0, 31               ; encoding: [0x5c,0x1f]
slo %r7, 31
# CHECK: slo %r7, 31               ; encoding: [0x5c,0xff]

# ==================== Branch Maximum/Minimum Displacement ====================

br 510
# CHECK: br 510                    ; encoding: [0x8e,0xff]
br -512
# CHECK: br -512                   ; encoding: [0x9e,0x00]
br 0
# CHECK: br 0                      ; encoding: [0x8e,0x00]

# All branch conditions with disp=0
beq 0
# CHECK: beq 0                     ; encoding: [0x80,0x00]
bne 0
# CHECK: bne 0                     ; encoding: [0x81,0x00]
blt 0
# CHECK: blt 0                     ; encoding: [0x8a,0x00]
bge 0
# CHECK: bge 0                     ; encoding: [0x8b,0x00]
bltu 0
# CHECK: bltu 0                    ; encoding: [0x84,0x00]
bgeu 0
# CHECK: bgeu 0                    ; encoding: [0x85,0x00]

# ==================== SAF Edge Cases ====================

# CALL maximum displacement (12-bit signed)
call 0
# CHECK: call 0                    ; encoding: [0xb0,0x00]
call 4094
# CHECK: call 4094                 ; encoding: [0xb7,0xff]
call -2048
# CHECK: call -2048                ; encoding: [0xbc,0x00]

# PUSH/POP all registers
push %r0
# CHECK: push %r0                  ; encoding: [0x1d,0xc0]
push %r1
# CHECK: push %r1                  ; encoding: [0x1d,0xc4]
push %r2
# CHECK: push %r2                  ; encoding: [0x1d,0xc8]
push %r3
# CHECK: push %r3                  ; encoding: [0x1d,0xcc]
push %r4
# CHECK: push %r4                  ; encoding: [0x1d,0xd0]
push %r5
# CHECK: push %r5                  ; encoding: [0x1d,0xd4]
push %r6
# CHECK: push %r6                  ; encoding: [0x1d,0xd8]
push %r7
# CHECK: push %r7                  ; encoding: [0x1d,0xdc]

pop %r0
# CHECK: pop %r0                   ; encoding: [0x1c,0x18]
pop %r1
# CHECK: pop %r1                   ; encoding: [0x1c,0x38]
pop %r2
# CHECK: pop %r2                   ; encoding: [0x1c,0x58]
pop %r3
# CHECK: pop %r3                   ; encoding: [0x1c,0x78]
pop %r4
# CHECK: pop %r4                   ; encoding: [0x1c,0x98]
pop %r5
# CHECK: pop %r5                   ; encoding: [0x1c,0xb8]
pop %r6
# CHECK: pop %r6                   ; encoding: [0x1c,0xd8]
pop %r7
# CHECK: pop %r7                   ; encoding: [0x1c,0xf8]

# PUSH immediate edge cases
push 0
# CHECK: push 0                    ; encoding: [0x5d,0xc0]
push 31
# CHECK: push 31                   ; encoding: [0x5d,0xdf]

# JMPR/CALLR all registers
jmpr %r0
# CHECK: jmpr %r0                  ; encoding: [0xaf,0x0e]
jmpr %r1
# CHECK: jmpr %r1                  ; encoding: [0xaf,0x2e]
jmpr %r2
# CHECK: jmpr %r2                  ; encoding: [0xaf,0x4e]
jmpr %r3
# CHECK: jmpr %r3                  ; encoding: [0xaf,0x6e]
jmpr %r4
# CHECK: jmpr %r4                  ; encoding: [0xaf,0x8e]
jmpr %r5
# CHECK: jmpr %r5                  ; encoding: [0xaf,0xae]
jmpr %r6
# CHECK: jmpr %r6                  ; encoding: [0xaf,0xce]
jmpr %r7
# CHECK: jmpr %r7                  ; encoding: [0xaf,0xee]

callr %r0
# CHECK: callr %r0                 ; encoding: [0xaf,0x1e]
callr %r1
# CHECK: callr %r1                 ; encoding: [0xaf,0x3e]
callr %r2
# CHECK: callr %r2                 ; encoding: [0xaf,0x5e]
callr %r3
# CHECK: callr %r3                 ; encoding: [0xaf,0x7e]
callr %r4
# CHECK: callr %r4                 ; encoding: [0xaf,0x9e]
callr %r5
# CHECK: callr %r5                 ; encoding: [0xaf,0xbe]
callr %r6
# CHECK: callr %r6                 ; encoding: [0xaf,0xde]
callr %r7
# CHECK: callr %r7                 ; encoding: [0xaf,0xfe]

# ==================== Mixed-width register combinatinos (same size on both sides) ====================

# Word RR - all opcodes with r0, r1
add %r0, %r1
# CHECK: add %r0, %r1              ; encoding: [0x10,0x04]
sub %r0, %r1
# CHECK: sub %r0, %r1              ; encoding: [0x11,0x04]
rsub %r0, %r1
# CHECK: rsub %r0, %r1             ; encoding: [0x12,0x04]
cmp %r0, %r1
# CHECK: cmp %r0, %r1              ; encoding: [0x13,0x04]
or %r0, %r1
# CHECK: or %r0, %r1               ; encoding: [0x14,0x04]
xor %r0, %r1
# CHECK: xor %r0, %r1              ; encoding: [0x15,0x04]
and %r0, %r1
# CHECK: and %r0, %r1              ; encoding: [0x16,0x04]
test %r0, %r1
# CHECK: test %r0, %r1             ; encoding: [0x17,0x04]
movz %r0, %r1
# CHECK: movz %r0, %r1             ; encoding: [0x18,0x04]
movs %r0, %r1
# CHECK: movs %r0, %r1             ; encoding: [0x19,0x04]
load %r0, %r1
# CHECK: load %r0, %r1             ; encoding: [0x1a,0x04]
store %r0, %r1
# CHECK: store %r0, %r1            ; encoding: [0x1b,0x04]

# ==================== NOP ====================
nop
# CHECK: nop                       ; encoding: [0x8f,0x00]
