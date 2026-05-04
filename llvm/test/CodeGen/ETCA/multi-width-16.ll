; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s --check-prefix=CHECK16
; RUN: llc -march=etca -mcpu=etca32 < %s | FileCheck %s --check-prefix=CHECK32
; RUN: llc -march=etca -mcpu=etca64 < %s | FileCheck %s --check-prefix=CHECK64

;; ===========================================================================
;; ETCA 16-bit arithmetic tests — ALL CPUs
;;
;; Tests that 16-bit arithmetic works on all word-size configurations.
;; On wider CPUs, 16-bit operands need register-copy bridging (MOVZ to/from
;; the wider register class).
;; ===========================================================================

define i16 @add_i16(i16 %a, i16 %b) {
; CHECK16-LABEL: add_i16:
; CHECK16:       add %r0, %r1
; CHECK16:       jmpr %r7
;
; CHECK32-LABEL: add_i16:
; CHECK32:       movz %r0, %r0d
; CHECK32:       movz %r1, %r1d
; CHECK32:       add %r0, %r1
; CHECK32:       movz %r0d, %r0
; CHECK32:       jmpr %r7
;
; CHECK64-LABEL: add_i16:
; CHECK64:       movz %r0, %r0q
; CHECK64:       movz %r1, %r1q
; CHECK64:       add %r0, %r1
; CHECK64:       movz %r0q, %r0
; CHECK64:       jmpr %r7
  %r = add i16 %a, %b
  ret i16 %r
}

define i16 @sub_i16(i16 %a, i16 %b) {
; CHECK16-LABEL: sub_i16:
; CHECK16:       sub %r0, %r1
; CHECK16:       jmpr %r7
;
; CHECK32-LABEL: sub_i16:
; CHECK32:       movz %r0, %r0d
; CHECK32:       movz %r1, %r1d
; CHECK32:       sub %r0, %r1
; CHECK32:       movz %r0d, %r0
; CHECK32:       jmpr %r7
;
; CHECK64-LABEL: sub_i16:
; CHECK64:       movz %r0, %r0q
; CHECK64:       movz %r1, %r1q
; CHECK64:       sub %r0, %r1
; CHECK64:       movz %r0q, %r0
; CHECK64:       jmpr %r7
  %r = sub i16 %a, %b
  ret i16 %r
}

define i16 @or_i16_const(i16 %a) {
; CHECK16-LABEL: or_i16_const:
; CHECK16:       movz %r1, 7
; CHECK16:       or %r0, %r1
; CHECK16:       jmpr %r7
;
; CHECK32-LABEL: or_i16_const:
; CHECK32:       movz %r0, %r0d
; CHECK32:       movz %r1, 7
; CHECK32:       or %r0, %r1
; CHECK32:       movz %r0d, %r0
; CHECK32:       jmpr %r7
;
; CHECK64-LABEL: or_i16_const:
; CHECK64:       movz %r0, %r0q
; CHECK64:       movz %r1, 7
; CHECK64:       or %r0, %r1
; CHECK64:       movz %r0q, %r0
; CHECK64:       jmpr %r7
  %r = or i16 %a, 7
  ret i16 %r
}
