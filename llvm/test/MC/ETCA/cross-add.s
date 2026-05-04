; Cross-assembler test: verify LLVM and binutils produce identical .text bytes.
;
; RUN: %etca-as %s -o %t.bin.o
; RUN: llvm-mc -arch=etca -mcpu=generic -filetype=obj %s -o %t.llvm.o
; RUN: %etca-objcopy -O binary --only-section=.text %t.bin.o %t.bin.text
; RUN: llvm-objcopy -O binary --only-section=.text %t.llvm.o %t.llvm.text
; RUN: cmp %t.bin.text %t.llvm.text

.text
  add %r0, %r1
  sub %r2, %r3
  or %r4, %r5
  xor %r6, %r7
  and %r0, %r1
  movz %r2, %r3
  movs %r4, %r5
  add %r0, 1
  sub %r1, -16
  movz %r0, 31
