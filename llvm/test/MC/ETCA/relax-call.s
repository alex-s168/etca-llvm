# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic %s -o %t.o
# RUN: ld.lld -m elf32etca %t.o -o %t.elf 2>&1
# RUN: llvm-objdump -d %t.elf | FileCheck %s
#
# MC relaxation: out-of-range CALL expanded to MOV_* + CALLR.

# CHECK:      <_start>:
# CHECK-NOT:  call
# Expanded form ends with jmpr:
# CHECK:      jmpr %r7

# CHECK:      target
# CHECK:      jmpr %r7

.text
.globl _start
_start:
  call target
  .space 6000
target:
  jmpr %r7
