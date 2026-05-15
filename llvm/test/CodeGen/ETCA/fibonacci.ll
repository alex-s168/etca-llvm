; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca32 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca64 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca32p64 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca64p32 < %s | FileCheck %s

;; ===========================================================================
;; ETCA fibonacci codegen test
;;
;; Tests recursive function calls with the Fibonacci sequence.
;; ===========================================================================

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
