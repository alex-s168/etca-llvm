; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca32 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca64 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca32p64 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca64p32 < %s | FileCheck %s

;; ===========================================================================
;; ETCA control-flow codegen tests
;;
;; Tests all conditional branch instructions generated from LLVM IR
;; icmp + br patterns.  Each test exercises one condition code.  The ETCA
;; backend lowers BR_CC by emitting a CMP followed by the matching
;; conditional branch.  When the icmp result feeds directly into a br,
;; SelectionDAGBuilder fuses them into a single BR_CC node.
;;
;; Branch mnemonics:
;;   BR   (unconditional),   BEQ, BNE,  BLT,  BGE,  BGT,  BLE,
;;   BLTU, BGEU, BGTU, BLEU
;; ===========================================================================

;; --- Unconditional branch ---

define i16 @branch_uncond(i16 %a) {
; CHECK-LABEL: branch_uncond:
; CHECK:       jmpr %r7
  br label %end
end:
  ret i16 %a
}

;; --- icmp eq ---

define i16 @branch_eq(i16 %a, i16 %b) {
; CHECK-LABEL: branch_eq:
; CHECK:       cmp %r0, %r1
; CHECK:       beq
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp eq i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp ne ---

define i16 @branch_ne(i16 %a, i16 %b) {
; CHECK-LABEL: branch_ne:
; CHECK:       cmp %r0, %r1
; CHECK:       bne
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp ne i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp slt (signed less than) ---

define i16 @branch_slt(i16 %a, i16 %b) {
; CHECK-LABEL: branch_slt:
; CHECK:       cmp %r0, %r1
; CHECK:       blt
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp slt i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp sge (signed greater or equal) ---

define i16 @branch_sge(i16 %a, i16 %b) {
; CHECK-LABEL: branch_sge:
; CHECK:       cmp %r0, %r1
; CHECK:       bge
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp sge i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp sgt (signed greater than) ---

define i16 @branch_sgt(i16 %a, i16 %b) {
; CHECK-LABEL: branch_sgt:
; CHECK:       cmp %r0, %r1
; CHECK:       bgt
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp sgt i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp sle (signed less or equal) ---

define i16 @branch_sle(i16 %a, i16 %b) {
; CHECK-LABEL: branch_sle:
; CHECK:       cmp %r0, %r1
; CHECK:       ble
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp sle i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp ult (unsigned less than) ---

define i16 @branch_ult(i16 %a, i16 %b) {
; CHECK-LABEL: branch_ult:
; CHECK:       cmp %r0, %r1
; CHECK:       bltu
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp ult i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp uge (unsigned greater or equal) ---

define i16 @branch_uge(i16 %a, i16 %b) {
; CHECK-LABEL: branch_uge:
; CHECK:       cmp %r0, %r1
; CHECK:       bgeu
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp uge i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp ugt (unsigned greater than) ---

define i16 @branch_ugt(i16 %a, i16 %b) {
; CHECK-LABEL: branch_ugt:
; CHECK:       cmp %r0, %r1
; CHECK:       bgtu
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp ugt i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp ule (unsigned less or equal) ---

define i16 @branch_ule(i16 %a, i16 %b) {
; CHECK-LABEL: branch_ule:
; CHECK:       cmp %r0, %r1
; CHECK:       bleu
; CHECK:       br
; CHECK:       jmpr %r7
; CHECK:       jmpr %r7
  %cmp = icmp ule i16 %a, %b
  br i1 %cmp, label %then, label %else
then:
  ret i16 %a
else:
  ret i16 %b
}

;; --- icmp with immediate operand ---

define i16 @branch_eq_imm(i16 %a) {
; CHECK-LABEL: branch_eq_imm:
; CHECK:       movz %r0, 0
; CHECK:       cmp %r1, %r0
; CHECK:       beq
; CHECK:       br
; CHECK:       movz %r0, 1
; CHECK:       jmpr %r7
  %cmp = icmp eq i16 %a, 0
  br i1 %cmp, label %then, label %else
then:
  ret i16 1
else:
  ret i16 0
}

define i16 @branch_sgt_imm(i16 %a) {
; CHECK-LABEL: branch_sgt_imm:
; CHECK:       movz %r0, 0
; CHECK:       cmp %r1, %r0
; CHECK:       bgt
; CHECK:       br
; CHECK:       movz %r0, 1
; CHECK:       jmpr %r7
  %cmp = icmp sgt i16 %a, 0
  br i1 %cmp, label %then, label %else
then:
  ret i16 1
else:
  ret i16 0
}

;; --- Select (conditional move) pattern ---

define i16 @select_eq(i16 %a, i16 %b, i16 %c) {
; CHECK-LABEL: select_eq:
; CHECK:       sub %r6
; CHECK:       add %r5
; CHECK:       store %r3, %r5
; CHECK:       add %r5
; CHECK:       cmp
; CHECK:       beq
; CHECK:       br
; CHECK:       movz %r0
; CHECK:       jmpr %r7
  %cmp = icmp eq i16 %a, 0
  %res = select i1 %cmp, i16 %b, i16 %c
  ret i16 %res
}

;; --- Loop with counter ---

define i16 @while_loop(i16 %n) {
; CHECK-LABEL: while_loop:
; CHECK:       sub %r6
; CHECK:       add %r1, %r2
; CHECK:       cmp %r3, %r0
; CHECK:       blt
; CHECK:       jmpr %r7
  br label %loop
loop:
  %i = phi i16 [ 0, %0 ], [ %next, %loop ]
  %next = add i16 %i, 1
  %cmp = icmp slt i16 %next, %n
  br i1 %cmp, label %loop, label %exit
exit:
  ret i16 %i
}

;; --- Nested control flow ---

define i16 @nested_branch(i16 %a, i16 %b, i16 %c) {
; CHECK-LABEL: nested_branch:
; CHECK:       cmp
; CHECK:       bge
; CHECK:       cmp
; CHECK:       bne
; CHECK:       movz %r0, 1
; CHECK:       movz %r0, 0
; CHECK:       jmpr %r7
  %outer = icmp sge i16 %a, %b
  br i1 %outer, label %inner_check, label %fail
inner_check:
  %inner = icmp ne i16 %b, %c
  br i1 %inner, label %pass, label %fail
pass:
  ret i16 1
fail:
  ret i16 0
}
