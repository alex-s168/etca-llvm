# RUN: llvm-mc -arch=etca -show-encoding < %s | FileCheck %s

# Comprehensive roundtrip test covering all major instruction groups.

# === RR instructions ===
add %r0, %r1
# CHECK: add %r0, %r1              ; encoding: [0x10,0x04]
sub %r3, %r5
# CHECK: sub %r3, %r5              ; encoding: [0x11,0x74]
rsub %r7, %r0
# CHECK: rsub %r7, %r0             ; encoding: [0x12,0xe0]
or %r5, %r2
# CHECK: or %r5, %r2               ; encoding: [0x14,0xa8]
xor %r6, %r4
# CHECK: xor %r6, %r4              ; encoding: [0x15,0xd0]
and %r1, %r7
# CHECK: and %r1, %r7              ; encoding: [0x16,0x3c]
movz %r0, %r7
# CHECK: movz %r0, %r7             ; encoding: [0x18,0x1c]
movs %r6, %r1
# CHECK: movs %r6, %r1             ; encoding: [0x19,0xc4]

# === Multi-width RR ===
add %r0d, %r1d
# CHECK: add %r0d, %r1d            ; encoding: [0x20,0x04]
add %r0q, %r1q
# CHECK: add %r0q, %r1q            ; encoding: [0x30,0x04]

# === RI instructions ===
add %r0, 5
# CHECK: add %r0, 5                ; encoding: [0x50,0x05]
sub %r5, 3
# CHECK: sub %r5, 3                ; encoding: [0x51,0xa3]
or %r7, 15
# CHECK: or %r7, 15                ; encoding: [0x54,0xef]
movz %r0, 31
# CHECK: movz %r0, 31              ; encoding: [0x58,0x1f]
slo %r1, 7
# CHECK: slo %r1, 7                ; encoding: [0x5c,0x27]
readcr %r0, 3
# CHECK: readcr %r0, 3             ; encoding: [0x5e,0x03]
cmp %r0, 5
# CHECK: cmp %r0, 5                ; encoding: [0x53,0x05]

# === LOAD/STORE ===
load %r0, %r1
# CHECK: load %r0, %r1             ; encoding: [0x1a,0x04]
store %r7, %r2
# CHECK: store %r7, %r2            ; encoding: [0x1b,0xe8]
cmp %r6, %r3
# CHECK: cmp %r6, %r3              ; encoding: [0x13,0xcc]
test %r7, %r0
# CHECK: test %r7, %r0             ; encoding: [0x17,0xe0]

# === Branch instructions ===
br 10
# CHECK: br 10                     ; encoding: [0x8e,0x05]
beq 8
# CHECK: beq 8                     ; encoding: [0x80,0x04]
bne 6
# CHECK: bne 6                     ; encoding: [0x81,0x03]
blt 4
# CHECK: blt 4                     ; encoding: [0x8a,0x02]
bge 12
# CHECK: bge 12                    ; encoding: [0x8b,0x06]
bltu 8
# CHECK: bltu 8                    ; encoding: [0x84,0x04]
bgtu 4
# CHECK: bgtu 4                    ; encoding: [0x89,0x02]

# === SAF instructions ===
push %r3
# CHECK: push %r3                  ; encoding: [0x1d,0xcc]
pop %r0
# CHECK: pop %r0                   ; encoding: [0x1c,0x18]
push 31
# CHECK: push 31                   ; encoding: [0x5d,0xdf]
jmpr %r7
# CHECK: jmpr %r7                  ; encoding: [0xaf,0xee]
callr %r0
# CHECK: callr %r0                 ; encoding: [0xaf,0x1e]

# === NOP ===
nop
# CHECK: nop                       ; encoding: [0x8f,0x00]
