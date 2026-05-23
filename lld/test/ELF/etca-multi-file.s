# REQUIRES: etca
# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic %p/Inputs/ETCA/lib.s -o %t.lib.o
# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic %s -o %t.main.o
# RUN: ld.lld -m elf32etca -o %t.exe %t.main.o %t.lib.o
# RUN: llvm-objdump -d %t.exe | FileCheck %s

# Test multi-file linking with external symbol resolution.

# CHECK:       <_start>:
# CHECK-NEXT:     movz
# CHECK-NEXT:     call
# CHECK:       <double_it>:
# CHECK-NEXT:     add

.text
.globl _start
_start:
  mov r1, 21
  call double_it
  br _start
