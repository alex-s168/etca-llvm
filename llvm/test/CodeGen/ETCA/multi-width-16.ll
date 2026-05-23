; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32,+dw < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64,+dw,+qw < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64,+dw,+qw < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32,+dw,+qw < %s | FileCheck %s

;; ===========================================================================
;; ETCA 16-bit arithmetic tests — ALL CPUs
;;
;; Tests that 16-bit arithmetic works on all word-size configurations.
;; ===========================================================================

define i16 @add_i16(i16 %a, i16 %b) {
; CHECK-LABEL: add_i16:
; CHECK:       add %r0, %r1
; CHECK:       jmpr %r7
  %r = add i16 %a, %b
  ret i16 %r
}

define i16 @sub_i16(i16 %a, i16 %b) {
; CHECK-LABEL: sub_i16:
; CHECK:       sub %r0, %r1
; CHECK:       jmpr %r7
  %r = sub i16 %a, %b
  ret i16 %r
}

define i16 @or_i16_const(i16 %a) {
; CHECK-LABEL: or_i16_const:
; CHECK:       movz %r1, 7
; CHECK:       or %r0, %r1
; CHECK:       jmpr %r7
  %r = or i16 %a, 7
  ret i16 %r
}
