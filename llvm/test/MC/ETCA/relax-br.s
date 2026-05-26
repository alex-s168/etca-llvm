# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic %s -o %t.o
# RUN: ld.lld -m elf32etca %t.o -o %t.elf 2>&1
# RUN: llvm-objdump -d %t.elf | FileCheck %s
#
# MC relaxation: out-of-range BR expanded to MOV_* + JMPR.

# CHECK:      <_start>:
# CHECK-NOT:  br
# Expanded form ends with jmpr:
# CHECK:      jmpr %r7

# CHECK:      target
# CHECK:      jmpr %r7

.text
.globl _start
_start:
  br target
  .space 600
target:
  jmpr %r7
