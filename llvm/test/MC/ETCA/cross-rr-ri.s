; Cross-test: all base RR and RI instructions
;
; RUN: %etca-as %s -o %t.bin.o
; RUN: llvm-mc -arch=etca -mcpu=generic -filetype=obj %s -o %t.llvm.o
; RUN: %etca-objcopy -O binary --only-section=.text %t.bin.o %t.bin.text
; RUN: llvm-objcopy -O binary --only-section=.text %t.llvm.o %t.llvm.text
; RUN: cmp %t.bin.text %t.llvm.text

.text
  ; All RR ops with various registers
  add %r0, %r1
  sub %r0, %r1
  rsub %r0, %r1
  or %r0, %r1
  xor %r0, %r1
  and %r0, %r1
  movz %r0, %r1
  movs %r0, %r1
  add %r3, %r5
  sub %r7, %r2
  or %r6, %r4
  xor %r1, %r0
  and %r5, %r7
  movz %r3, %r6
  movs %r2, %r4

  ; CMP/TEST
  cmp %r0, %r1
  test %r0, %r1
  cmp %r6, %r3
  test %r7, %r0
  cmp %r2, %r5
  test %r3, %r7

  ; RI ops
  add %r0, 0
  add %r0, 15
  add %r0, -1
  add %r0, -16
  sub %r0, 1
  sub %r0, -16
  rsub %r0, 0
  rsub %r2, 15
  or %r5, 3
  xor %r7, 7
  and %r1, 15
  movz %r0, 31
  movs %r0, -1

  ; CMPI/TESTI
  cmp %r0, 5
  test %r4, 0
  cmp %r6, 15
  test %r1, -8

  ; SLO
  slo %r0, 31
  slo %r3, 0

  ; READCR/WRITECR
  readcr %r0, 3
  readcr %r1, 7
  writecr %r0, 3
  writecr %r2, 15
