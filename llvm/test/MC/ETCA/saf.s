# RUN: llvm-mc -arch=etca -mattr=+saf -show-encoding < %s | FileCheck %s

# Test SAF extension instructions.

# === PUSH register (RR format: rA=6(sp), rB=Reg, CCCC=0xD) ===
push %r0
# CHECK: push %r0                  ; encoding: [0x1d,0xc0]

push %r7
# CHECK: push %r7                  ; encoding: [0x1d,0xdc]

push %r3
# CHECK: push %r3                  ; encoding: [0x1d,0xcc]

# === POP register (RR format: rA=Reg, rB=6(sp), CCCC=0xC) ===
pop %r0
# CHECK: pop %r0                   ; encoding: [0x1c,0x18]

pop %r7
# CHECK: pop %r7                   ; encoding: [0x1c,0xf8]

# === PUSH immediate (RI format: rA=6(sp), imm, CCCC=0xD) ===
push 5
# CHECK: push 5                    ; encoding: [0x5d,0xc5]

push 31
# CHECK: push 31                   ; encoding: [0x5d,0xdf]

# === CALL (12-bit displacement) ===
call 0
# CHECK: call 0                    ; encoding: [0xb0,0x00]

call 8
# CHECK: call 8                    ; encoding: [0xb0,0x04]

# === JMPR (register jump) ===
jmpr %r0
# CHECK: jmpr %r0                  ; encoding: [0xaf,0x0e]

jmpr %r7
# CHECK: jmpr %r7                  ; encoding: [0xaf,0xee]

# === CALLR (register call) ===
callr %r0
# CHECK: callr %r0                 ; encoding: [0xaf,0x1e]

callr %r3
# CHECK: callr %r3                 ; encoding: [0xaf,0x7e]
