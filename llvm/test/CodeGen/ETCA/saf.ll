; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca32 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca64 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca32p64 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca64p32 < %s | FileCheck %s

;; ===========================================================================
;; ETCA SAF extension codegen tests
;; ===========================================================================

declare void @callee()
declare void @foo()
declare void @bar()

define void @caller() {
; CHECK-LABEL: caller:
; CHECK:       call callee
; CHECK:       jmpr %r7
  call void @callee()
  ret void
}

define i16 @recursion(i16 %n) {
; CHECK-LABEL: recursion:
; CHECK:       cmp
; CHECK:       beq
; CHECK:       br
; CHECK:       call recursion
; CHECK:       add %r0
; CHECK:       jmpr %r7
  %1 = icmp eq i16 %n, 0
  br i1 %1, label %base, label %recurse

recurse:
  %n1 = sub i16 %n, 1
  %r = call i16 @recursion(i16 %n1)
  %sum = add i16 %r, %n
  ret i16 %sum

base:
  ret i16 1
}

define i16 @fib(i16 %n) {
; CHECK-LABEL: fib:
; CHECK:       movz %r5, %r6
; CHECK:       cmp
; CHECK:       bltu
; CHECK:       br
; CHECK:       call fib
; CHECK:       call fib
; CHECK:       add %r0
; CHECK:       jmpr %r7
  %1 = icmp ult i16 %n, 2
  br i1 %1, label %return, label %recurse

recurse:
  %n1 = sub i16 %n, 1
  %f1 = call i16 @fib(i16 %n1)
  %n2 = sub i16 %n, 2
  %f2 = call i16 @fib(i16 %n2)
  %r = add i16 %f1, %f2
  ret i16 %r

return:
  ret i16 %n
}

define void @multi_call() {
; CHECK-LABEL: multi_call:
; CHECK:       call foo
; CHECK:       call bar
; CHECK:       jmpr %r7
  call void @foo()
  call void @bar()
  ret void
}

define i16 @branch_eq(i16 %a, i16 %b) {
; CHECK-LABEL: branch_eq:
; CHECK:       movz %r5, %r6
; CHECK:       cmp
; CHECK:       beq
; CHECK-NOT:   cmp
; CHECK:       jmpr %r7
  %cmp = icmp eq i16 %a, %b
  %res = select i1 %cmp, i16 1, i16 0
  ret i16 %res
}

define i16 @branch_slt(i16 %a, i16 %b) {
; CHECK-LABEL: branch_slt:
; CHECK:       cmp
; CHECK:       blt
; CHECK-NOT:   cmp
; CHECK:       jmpr %r7
  %cmp = icmp slt i16 %a, %b
  %res = select i1 %cmp, i16 1, i16 0
  ret i16 %res
}

define i16 @branch_ult(i16 %a, i16 %b) {
; CHECK-LABEL: branch_ult:
; CHECK:       cmp
; CHECK:       bltu
; CHECK-NOT:   cmp
; CHECK:       jmpr %r7
  %cmp = icmp ult i16 %a, %b
  %res = select i1 %cmp, i16 1, i16 0
  ret i16 %res
}

define i16 @branch_sgt(i16 %a, i16 %b) {
; CHECK-LABEL: branch_sgt:
; CHECK:       cmp
; CHECK:       bgt
; CHECK-NOT:   cmp
; CHECK:       jmpr %r7
  %cmp = icmp sgt i16 %a, %b
  %res = select i1 %cmp, i16 1, i16 0
  ret i16 %res
}

define i16 @loop(i16 %limit) {
; CHECK-LABEL: loop:
; CHECK:       movz %r5, %r6
; CHECK:       add
; CHECK:       cmp
; CHECK:       blt
; CHECK:       jmpr %r7
  br label %loop

loop:
  %i = phi i16 [0, %0], [%next, %loop]
  %sum = phi i16 [65535, %0], [%sum2, %loop]
  %next = add i16 %i, 1
  %sum2 = add i16 %sum, %next
  %cond = icmp slt i16 %sum2, %limit
  br i1 %cond, label %loop, label %exit

exit:
  ret i16 %sum
}
