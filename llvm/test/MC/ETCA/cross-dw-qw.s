; Cross-assembler test: verify LLVM and binutils produce identical .text bytes
; for dword (32-bit) and qword (64-bit) instructions.
;
; NOTE: This test uses the binutils assembler (etca-unknown-elf-as) to assemble
; with DWQW extensions, then compares the binary output with LLVM's output.
; The source uses LLVM syntax (%rNd/%rNq). A sed translation converts to
; binutils syntax (%rdN/%rqN) for the binutils side.
;
; Requires: etca-unknown-elf-binutils with DWQW extension support.
; If not available, the test is skipped by lit.local.cfg.
;
; RUN: sed '/^;/d; s/%r\([0-7]\)d/%rd\1/g; s/%r\([0-7]\)q/%rq\1/g' %s | \
; RUN:   %etca-as -march=base+DW,QW -o %t.bin.o 2>%t.bin.err
; RUN: %etca-objcopy -O binary --only-section=.text %t.bin.o %t.bin.text
; RUN: llvm-mc -arch=etca -filetype=obj %s -o %t.llvm.o
; RUN: llvm-objcopy -O binary --only-section=.text %t.llvm.o %t.llvm.text
; RUN: cmp %t.bin.text %t.llvm.text

.text
  ; === DWORD RR Instructions ===
  add %r0d, %r1d
  sub %r2d, %r3d
  rsub %r4d, %r5d
  cmp %r0d, %r1d
  or %r0d, %r1d
  xor %r6d, %r7d
  and %r0d, %r1d
  test %r6d, %r7d
  movz %r2d, %r3d
  movs %r4d, %r5d

  ; === QWORD RR Instructions ===
  add %r0q, %r1q
  sub %r2q, %r3q
  rsub %r4q, %r5q
  cmp %r0q, %r1q
  or %r0q, %r1q
  xor %r6q, %r7q
  and %r0q, %r1q
  test %r6q, %r7q
  movz %r2q, %r3q
  movs %r4q, %r5q

  ; === DWORD RI Instructions ===
  add %r0d, 1
  sub %r1d, -2
  cmp %r0d, 5
  or %r3d, 4
  xor %r4d, 7
  and %r5d, 6
  test %r6d, 7
  movz %r0d, 15
  movs %r1d, -8

  ; === QWORD RI Instructions ===
  add %r0q, 1
  sub %r1q, -2
  cmp %r0q, 5
  or %r3q, 4
  xor %r4q, 7
  and %r5q, 6
  test %r6q, 7
  movz %r0q, 15
  movs %r1q, -8

  ; === DWORD LOAD/STORE (data=dword, addr=word for small model) ===
  load %r0d, %r1
  store %r7d, %r2

  ; === QWORD LOAD/STORE (data=qword, addr=word for small model) ===
  load %r0q, %r1
  store %r7q, %r2
