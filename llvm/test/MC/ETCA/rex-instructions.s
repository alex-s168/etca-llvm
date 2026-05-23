# RUN: llvm-mc -arch=etca -mcpu=generic -mattr=+rex --show-encoding %s 2>&1 | FileCheck %s
# RUN: llvm-mc -arch=etca -mcpu=generic -mattr=+rex %s -filetype=obj | llvm-objdump -d --arch-name=etca - | FileCheck --check-prefix=DISASM %s

# Test REX extension: expanded registers (r8-r15) with REX prefix emission.

# === 16-bit RR instructions with REX registers ===

# CHECK: add %r8, %r9                           ; encoding: [0xc6,0x10,0x04]
# DISASM: add %r8, %r9
    add %r8, %r9

# CHECK: sub %r10, %r11                         ; encoding: [0xc6,0x11,0x4c]
# DISASM: sub %r10, %r11
    sub %r10, %r11

# CHECK: and %r12, %r13                         ; encoding: [0xc6,0x16,0x94]
# DISASM: and %r12, %r13
    and %r12, %r13

# CHECK: or %r14, %r15                          ; encoding: [0xc6,0x14,0xdc]
# DISASM: or %r14, %r15
    or %r14, %r15

# CHECK: xor %r8, %r9                           ; encoding: [0xc6,0x15,0x04]
# DISASM: xor %r8, %r9
    xor %r8, %r9

# CHECK: movz %r10, %r11                        ; encoding: [0xc6,0x18,0x4c]
# DISASM: movz %r10, %r11
    movz %r10, %r11

# CHECK: movs %r12, %r13                        ; encoding: [0xc6,0x19,0x94]
# DISASM: movs %r12, %r13
    movs %r12, %r13

# CHECK: cmp %r14, %r15                         ; encoding: [0xc6,0x13,0xdc]
# DISASM: cmp %r14, %r15
    cmp %r14, %r15

# CHECK: test %r8, %r9                          ; encoding: [0xc6,0x17,0x04]
# DISASM: test %r8, %r9
    test %r8, %r9

# === 32-bit RR instructions with REX registers ===

# CHECK: add %r8d, %r9d                         ; encoding: [0xc6,0x20,0x04]
# DISASM: add %r8d, %r9d
    add %r8d, %r9d

# CHECK: sub %r10d, %r11d                       ; encoding: [0xc6,0x21,0x4c]
# DISASM: sub %r10d, %r11d
    sub %r10d, %r11d

# CHECK: xor %r14d, %r15d                       ; encoding: [0xc6,0x25,0xdc]
# DISASM: xor %r14d, %r15d
    xor %r14d, %r15d

# CHECK: movz %r12d, %r13d                      ; encoding: [0xc6,0x28,0x94]
# DISASM: movz %r12d, %r13d
    movz %r12d, %r13d

# === 64-bit RR instructions with REX registers ===

# CHECK: add %r8q, %r9q                         ; encoding: [0xc6,0x30,0x04]
# DISASM: add %r8q, %r9q
    add %r8q, %r9q

# CHECK: sub %r10q, %r11q                       ; encoding: [0xc6,0x31,0x4c]
# DISASM: sub %r10q, %r11q
    sub %r10q, %r11q

# CHECK: movz %r12q, %r13q                      ; encoding: [0xc6,0x38,0x94]
# DISASM: movz %r12q, %r13q
    movz %r12q, %r13q

# === REX RI instructions ===

# CHECK: add %r8, 7                             ; encoding: [0xc6,0x50,0x07]
# DISASM: add %r8, 7
    add %r8, 7

# CHECK: sub %r10, 5                            ; encoding: [0xc6,0x51,0x45]
# DISASM: sub %r10, 5
    sub %r10, 5

# CHECK: movz %r14, 31                          ; encoding: [0xc4,0x58,0xdf]
# DISASM: movz %r14, 31
    movz %r14, 31

# === REX LOAD/STORE ===

# CHECK: load %r8, %r9                          ; encoding: [0xc6,0x1a,0x04]
# DISASM: load %r8, %r9
    load %r8, %r9

# CHECK: store %r10, %r11                       ; encoding: [0xc6,0x1b,0x4c]
# DISASM: store %r10, %r11
    store %r10, %r11

# === REX PUSH/POP ===

# CHECK: push %r8                               ; encoding: [0xc2,0x1d,0xc0]
# DISASM: push %r8
    push %r8

# CHECK: pop %r9                                ; encoding: [0xc4,0x1c,0x38]
# DISASM: pop %r9
    pop %r9

# === Non-REX baseline (registers 0-7, no prefix) ===

# CHECK: add %r0, %r1                           ; encoding: [0x10,0x04]
# DISASM: add %r0, %r1
    add %r0, %r1

# CHECK: movz %r3, %r4                          ; encoding: [0x18,0x70]
# DISASM: movz %r3, %r4
    movz %r3, %r4

# CHECK: load %r0, %r1                          ; encoding: [0x1a,0x04]
# DISASM: load %r0, %r1
    load %r0, %r1

# CHECK: push %r0                               ; encoding: [0x1d,0xc0]
# DISASM: push %r0
    push %r0

# === ABI names (t0-t4, s2-s4) ===

# CHECK: add %r8, %r9                           ; encoding: [0xc6,0x10,0x04]
# DISASM: add %r8, %r9
    add %t0, %t1

# CHECK: add %r13, %r14                         ; encoding: [0xc6,0x10,0xb8]
# DISASM: add %r13, %r14
    add %s2, %s3
