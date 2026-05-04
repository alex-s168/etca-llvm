# RUN: llvm-mc -arch=etca -show-encoding < %s | FileCheck %s

# Test all register-register ALU instructions with 16-bit registers (r0-r7).
# RR format: (rA << 13) | (rB << 10) | (SS << 4) | CCCC

# --- Basic RR instructions (each with r0,r1) ---

# add %r0, %r1 → 0x0410 → [0x10,0x04]
add %r0, %r1
# CHECK: add %r0, %r1              ; encoding: [0x10,0x04]

# sub %r0, %r1 → 0x0411 → [0x11,0x04]
sub %r0, %r1
# CHECK: sub %r0, %r1              ; encoding: [0x11,0x04]

# rsub %r0, %r1 → 0x0412 → [0x12,0x04]
rsub %r0, %r1
# CHECK: rsub %r0, %r1             ; encoding: [0x12,0x04]

# or %r0, %r1 → 0x0414 → [0x14,0x04]
or %r0, %r1
# CHECK: or %r0, %r1               ; encoding: [0x14,0x04]

# xor %r0, %r1 → 0x0415 → [0x15,0x04]
xor %r0, %r1
# CHECK: xor %r0, %r1              ; encoding: [0x15,0x04]

# and %r0, %r1 → 0x0416 → [0x16,0x04]
and %r0, %r1
# CHECK: and %r0, %r1              ; encoding: [0x16,0x04]

# movz %r0, %r1 → 0x0418 → [0x18,0x04]
movz %r0, %r1
# CHECK: movz %r0, %r1             ; encoding: [0x18,0x04]

# movs %r0, %r1 → 0x0419 → [0x19,0x04]
movs %r0, %r1
# CHECK: movs %r0, %r1             ; encoding: [0x19,0x04]

# --- Different register combinations ---

# add %r3, %r5 → 0x7410 → [0x10,0x74]
add %r3, %r5
# CHECK: add %r3, %r5              ; encoding: [0x10,0x74]

# add %r7, %r7 → 0xFC10 → [0x10,0xfc]
add %r7, %r7
# CHECK: add %r7, %r7              ; encoding: [0x10,0xfc]

# or %r5, %r2 → 0xA814 → [0x14,0xA8]
or %r5, %r2
# CHECK: or %r5, %r2               ; encoding: [0x14,0xa8]

# and %r6, %r3 → 0xCC16 → [0x16,0xcc]
and %r6, %r3
# CHECK: and %r6, %r3              ; encoding: [0x16,0xcc]

# --- 32-bit RR (dword registers: %rNd) ---
# add %r0d, %r1d → 0x0420 → [0x20,0x04]
add %r0d, %r1d
# CHECK: add %r0d, %r1d            ; encoding: [0x20,0x04]

# sub %r3d, %r5d → 0x7421 → [0x21,0x74]
sub %r3d, %r5d
# CHECK: sub %r3d, %r5d            ; encoding: [0x21,0x74]

# --- 64-bit RR (qword registers: %rNq) ---
# add %r0q, %r1q → 0x0430 → [0x30,0x04]
add %r0q, %r1q
# CHECK: add %r0q, %r1q            ; encoding: [0x30,0x04]

# movz %r7q, %r6q → 0xF838 → [0x38,0xf8]
movz %r7q, %r6q
# CHECK: movz %r7q, %r6q           ; encoding: [0x38,0xf8]

# --- Backward compatibility: bare rN (no %) also works ---
add r0, r1
# CHECK: add %r0, %r1              ; encoding: [0x10,0x04]

# --- Backward compatibility: dN, qN also work ---
add d0, d1
# CHECK: add %r0d, %r1d            ; encoding: [0x20,0x04]

add q0, q1
# CHECK: add %r0q, %r1q            ; encoding: [0x30,0x04]
