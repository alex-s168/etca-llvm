; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s --check-prefix=CHECK16
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 < %s | FileCheck %s --check-prefix=CHECK32

;; ===========================================================================
;; ETCA sign-extension tests — 16-bit and 32-bit word sizes.
;; ===========================================================================

define i32 @zext_i16_to_i32(i16 %a) {
; CHECK16-LABEL: zext_i16_to_i32:
; CHECK16:       jmpr %r7
;
; CHECK32-LABEL: zext_i16_to_i32:
; CHECK32:       {{and %r[0-9]+d, %r[0-9]+d}}
; CHECK32:       jmpr %r7
  %ext = zext i16 %a to i32
  ret i32 %ext
}

define i32 @sext_i16_to_i32(i16 %a) {
; CHECK16-LABEL: sext_i16_to_i32:
; CHECK16:       jmpr %r7
;
; CHECK32-LABEL: sext_i16_to_i32:
; CHECK32:       movs %r0, %r0
; CHECK32:       jmpr %r7
  %ext = sext i16 %a to i32
  ret i32 %ext
}

define i16 @trunc_i32_to_i16(i32 %a) {
; CHECK16-LABEL: trunc_i32_to_i16:
; CHECK16:       movz %r0d, %r0d
; CHECK16:       jmpr %r7
;
; CHECK32-LABEL: trunc_i32_to_i16:
; CHECK32:       movs %r0, %r0
; CHECK32:       jmpr %r7
  %tr = trunc i32 %a to i16
  ret i16 %tr
}

define i32 @add_i16_sext_i32(i16 %a, i16 %b) {
; CHECK16-LABEL: add_i16_sext_i32:
; CHECK16:       add %r1, %r0
; CHECK16:       movz %r0d, %r1
; CHECK16:       jmpr %r7
;
; CHECK32-LABEL: add_i16_sext_i32:
; CHECK32:       add %r1, %r0
; CHECK32:       movs %r1, %r1
; CHECK32:       movz %r0d, %r1
; CHECK32:       jmpr %r7
  %sum = add i16 %a, %b
  %ext = sext i16 %sum to i32
  ret i32 %ext
}

define i32 @add_i16_zext_i32(i16 %a, i16 %b) {
; CHECK16-LABEL: add_i16_zext_i32:
; CHECK16:       add %r1, %r0
; CHECK16:       movz %r0d, %r1
; CHECK16:       jmpr %r7
;
; CHECK32-LABEL: add_i16_zext_i32:
; CHECK32:       add %r1, %r0
; CHECK32:       movz %r1, %r1
; CHECK32:       movz %r0d, %r1
; CHECK32:       jmpr %r7
  %sum = add i16 %a, %b
  %ext = zext i16 %sum to i32
  ret i32 %ext
}
