# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic -mattr=+64bit,+ptr64 %s -o %t.o
# RUN: ld.lld -m elf64etca %t.o -o %t.elf 2>&1
# RUN: llvm-objdump -d %t.elf | FileCheck %s
#
# Near-range branches with ptr64 should stay short.

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
