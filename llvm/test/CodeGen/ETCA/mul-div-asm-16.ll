; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s --check-prefix=GEN
; RUN: llc -march=etca -mcpu=etca32 < %s | FileCheck %s --check-prefix=GEN
; RUN: llc -march=etca -mcpu=etca64 < %s | FileCheck %s --check-prefix=GEN

;; Test that 16-bit MUL/DIV/REM operations are lowered to libcalls in
;; final assembly on all CPUs.

define i16 @mul16(i16 %a, i16 %b) {
; GEN-LABEL: mul16:
; GEN:       call __mulhi3
; GEN:       jmpr %r7
  %r = mul i16 %a, %b
  ret i16 %r
}

define i16 @sdiv16(i16 %a, i16 %b) {
; GEN-LABEL: sdiv16:
; GEN:       call __divhi3
; GEN:       jmpr %r7
  %r = sdiv i16 %a, %b
  ret i16 %r
}

define i16 @udiv16(i16 %a, i16 %b) {
; GEN-LABEL: udiv16:
; GEN:       call __udivhi3
; GEN:       jmpr %r7
  %r = udiv i16 %a, %b
  ret i16 %r
}

define i16 @srem16(i16 %a, i16 %b) {
; GEN-LABEL: srem16:
; GEN:       call __modhi3
; GEN:       jmpr %r7
  %r = srem i16 %a, %b
  ret i16 %r
}

define i16 @urem16(i16 %a, i16 %b) {
; GEN-LABEL: urem16:
; GEN:       call __umodhi3
; GEN:       jmpr %r7
  %r = urem i16 %a, %b
  ret i16 %r
}
