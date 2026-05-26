# RUN: llvm-mc -filetype=obj -arch=etca -mcpu=generic %s -o %t.o
# RUN: echo "SECTIONS {" > %t.script
# RUN: echo "  .text 0x100 : { *(.text.one) }" >> %t.script
# RUN: echo "  .text2 0x10000 : { *(.text.two) }" >> %t.script
# RUN: echo "}" >> %t.script
# RUN: ld.lld -T %t.script %t.o -o %t.elf 2>&1
# RUN: llvm-objdump -d %t.elf | FileCheck %s
#
# Test that out-of-range branches are relaxed by the linker.
# func1 is at 0x100 and func2 is at 0x10000 (far apart via .text/.text2).
# The 9-bit BR can't reach, so the linker expands to MOV_*+JMPR.
#
# CHECK:      Disassembly of section .text:
# CHECK:      func1
# The short BR must NOT appear (expanded by linker):
# CHECK-NOT:  br
# CHECK:      jmpr %r7
#
# CHECK:      Disassembly of section .text2:
# CHECK:      func2
# CHECK:      jmpr %r7

.section .text.one,"ax",@progbits
.globl func1
func1:
  br func2
  jmpr %r7

.section .text.two,"ax",@progbits
.globl func2
func2:
  jmpr %r7
