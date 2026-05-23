# REQUIRES: etca
# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic %s -o %t.o
# RUN: ld.lld -m elf32etca -o %t.exe %t.o
# RUN: llvm-readobj -h -r -s %t.exe | FileCheck %s
# RUN: llvm-objdump -d %t.exe | FileCheck --check-prefix=DISASM %s

# Test basic ETCA ELF linking with branch and call relocations.

# CHECK:      Format: elf32-{{.*}}
# CHECK:      Arch: etca
# CHECK-DAG: Machine: 0xE7CA
# CHECK-DAG: OS/ABI: Standalone

# DISASM:       <_start>:
# DISASM-NEXT:     br
# DISASM-NEXT:     call
# DISASM-NEXT:     br
# DISASM:       <helper>:

.text
.globl _start
_start:
  br helper
  call helper
  br _start

helper:
  add r1, r1
  jmpr ln
