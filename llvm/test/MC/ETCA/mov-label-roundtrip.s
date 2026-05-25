# RUN: llvm-mc -arch=etca -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DISASSM
# RUN: ld.lld -m elf32etca -o %t.exe %t.o
# RUN: llvm-objdump -d %t.exe | FileCheck %s --check-prefix=DISLINK

# Test that MOVZ/MOVS with label operands round-trip correctly through
# assembly, disassembly of the unlinked object, and after linking.
#
# The unlinked object has placeholder chains (immediates = 0).
# After linking, the MOV_* relocation rewrites the chain with the
# final absolute address.

.text
.globl _start
_start:
  movz %r0, target
  movs %r1, target

target:
  nop

# DISASSM:      <_start>:
# Unlinked: placeholder chains - immediates are 0
# DISASSM:      movz %r0, 0
# DISASSM:      slo %r0, 0
# DISASSM:      slo %r0, 0
# DISASSM:      slo %r0, 0
# DISASSM:      movs %r1,
# DISASSM:      slo %r1,
# DISASSM:      slo %r1,
# DISASSM:      slo %r1,
# DISASSM:      nop

# DISLINK:      <_start>:
# After linking: chain rewritten with final address of 'target'
# DISLINK:      movz %r0,
# DISLINK:      slo %r0,
# DISLINK:      slo %r0,
# DISLINK:      movz %r1,
# DISLINK:      slo %r1,
# DISLINK:      slo %r1,

# DISLINK:      <target>:
