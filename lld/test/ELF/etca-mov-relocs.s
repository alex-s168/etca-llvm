# REQUIRES: etca
# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic %s -o %t.o
# RUN: ld.lld -m elf32etca -o %t.exe %t.o
# RUN: llvm-objdump -d %t.exe | FileCheck %s

# Test that MOV_* relocations are correctly resolved by LLD.
# The linker rewrites the MOVZI+SLO placeholder chain in-place
# with the final absolute address via buildMovRi().

# CHECK:      <_start>:
# The 4-instruction chain (MOVZI + 3xSLO) is rewritten with the
# final absolute address of my_data, split into 5-bit chunks.
# CHECK:      movz %r0,
# CHECK:      slo %r0,
# CHECK:      nop
# CHECK:      nop
# CHECK:      load %r0, %r0
# CHECK:      jmpr %r7

.text
.globl _start
_start:
  movz %r0, my_data
  load %r0, %r0
  jmpr %r7

.data
my_data:
  .short 42
