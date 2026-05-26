# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic %s -o %t.o
# RUN: ld.lld -m elf32etca %t.o -o %t.elf 2>&1
# RUN: llvm-objdump -d %t.elf | FileCheck %s
#
# Near-range branches should stay in short form (NOT relaxed).

# CHECK:      <near_br>:
# CHECK:      br
# CHECK:      <near_call>:
# CHECK:      call
# CHECK:      target
# CHECK:      jmpr %r7

.text
.globl near_br
near_br:
  br target
  nop
near_call:
  call target
  nop
target:
  jmpr %r7
