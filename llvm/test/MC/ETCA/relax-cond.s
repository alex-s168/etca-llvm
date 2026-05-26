# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic %s -o %t.o
# RUN: ld.lld -m elf32etca %t.o -o %t.elf 2>&1
# RUN: llvm-objdump -d %t.elf | FileCheck %s
#
# MC relaxation: out-of-range conditional branches expanded to
# inverted-cond + MOV_* + JMPR.

# CHECK:      <test_beq>:
# CHECK-NOT:  beq
# CHECK:      jmpr %r7
# CHECK:      <test_bne>:
# CHECK-NOT:  bne
# CHECK:      jmpr %r7
# CHECK:      <test_blt>:
# CHECK-NOT:  blt
# CHECK:      jmpr %r7
# CHECK:      target
# CHECK:      jmpr %r7

.text
.globl test_beq
test_beq:
  movz %r0, 1
  cmp %r0, %r0
  beq target
  jmpr %r7
  .space 600
.globl test_bne
test_bne:
  movz %r0, 1
  cmp %r0, %r0
  bne target
  jmpr %r7
  .space 600
.globl test_blt
test_blt:
  movz %r0, 1
  cmp %r0, %r0
  blt target
  jmpr %r7
  .space 600
target:
  jmpr %r7
