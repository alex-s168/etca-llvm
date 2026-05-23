# REQUIRES: etca
# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=etca64 %s -o %t.o
# RUN: ld.lld -m elf64etca -o %t.exe %t.o
# RUN: llvm-readobj -h %t.exe | FileCheck %s
# RUN: llvm-objdump -d %t.exe | FileCheck --check-prefix=DISASM %s

# Test 64-bit ELF output for ETCA.

# CHECK:      Format: elf64-{{.*}}
# CHECK:      Arch: etca
# CHECK:      AddressSize: 64bit
# CHECK-DAG: Machine: 0xE7CA
# CHECK-DAG: Class: 64-bit

# DISASM:       <_start>:
# DISASM-NEXT:     call
# DISASM-NEXT:     br
# DISASM:       <helper>:

.text
.globl _start
_start:
  call helper
  br _start

helper:
  add r1, r1
  jmpr ln
