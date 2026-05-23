; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32,+dw < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64,+dw,+qw < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64,+dw,+qw < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32,+dw,+qw < %s | FileCheck %s

;; ===========================================================================
;; ETCA 32-bit arithmetic tests — CPUs with DW extension
;; ===========================================================================

define i32 @add_i32(i32 %a, i32 %b) {
; CHECK-LABEL: add_i32:
; CHECK:       add %r0d, %r1d
; CHECK:       jmpr %r7
  %r = add i32 %a, %b
  ret i32 %r
}

define i32 @and_i32(i32 %a, i32 %b) {
; CHECK-LABEL: and_i32:
; CHECK:       and %r0d, %r1d
; CHECK:       jmpr %r7
  %r = and i32 %a, %b
  ret i32 %r
}
