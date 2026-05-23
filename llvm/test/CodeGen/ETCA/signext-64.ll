; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64,+dw,+qw < %s | FileCheck %s --check-prefix=CHECK64
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32,+dw,+qw < %s | FileCheck %s --check-prefix=CHECK64

;; ===========================================================================
;; ETCA 64-bit sign-extension tests
;; ===========================================================================

define i64 @zext_i32_to_i64(i32 %a) {
; CHECK64-LABEL: zext_i32_to_i64:
; CHECK64:       {{and %r[0-9]+q, %r[0-9]+q}}
; CHECK64:       jmpr %r7
  %ext = zext i32 %a to i64
  ret i64 %ext
}

define i64 @sext_i32_to_i64(i32 %a) {
; CHECK64-LABEL: sext_i32_to_i64:
; CHECK64:       {{movz %r[0-9]+q, %r[0-9]+q}}
; CHECK64:       {{movs %r[0-9]+d, %r[0-9]+d}}
; CHECK64:       jmpr %r7
  %ext = sext i32 %a to i64
  ret i64 %ext
}

define i64 @zext_i16_to_i64(i16 %a) {
; CHECK64-LABEL: zext_i16_to_i64:
; CHECK64:       {{and %r[0-9]+q, %r[0-9]+q}}
; CHECK64:       jmpr %r7
  %ext = zext i16 %a to i64
  ret i64 %ext
}

define i64 @sext_i16_to_i64(i16 %a) {
; CHECK64-LABEL: sext_i16_to_i64:
; CHECK64:       {{movz %r[0-9]+q, %r[0-9]+q}}
; CHECK64:       movs %r0, %r0
; CHECK64:       jmpr %r7
  %ext = sext i16 %a to i64
  ret i64 %ext
}

define i32 @zext_i16_to_i32(i16 %a) {
; CHECK64-LABEL: zext_i16_to_i32:
; CHECK64:       {{and %r[0-9]+q, %r[0-9]+q}}
; CHECK64:       jmpr %r7
  %ext = zext i16 %a to i32
  ret i32 %ext
}

define i32 @sext_i16_to_i32(i16 %a) {
; CHECK64-LABEL: sext_i16_to_i32:
; CHECK64:       {{movz %r[0-9]+q, %r[0-9]+q}}
; CHECK64:       movs %r0, %r0
; CHECK64:       jmpr %r7
  %ext = sext i16 %a to i32
  ret i32 %ext
}

define i16 @trunc_i32_to_i16(i32 %a) {
; CHECK64-LABEL: trunc_i32_to_i16:
; CHECK64:       {{movz %r[0-9]+q, %r[0-9]+q}}
; CHECK64:       movs %r0, %r0
; CHECK64:       jmpr %r7
  %tr = trunc i32 %a to i16
  ret i16 %tr
}

define i32 @add_i16_sext_i32(i16 %a, i16 %b) {
; CHECK64-LABEL: add_i16_sext_i32:
; CHECK64:       {{add %r[0-9]+, %r[0-9]+}}
; CHECK64:       {{movs %r[0-9]+, %r[0-9]+}}
; CHECK64:       {{movz %r[0-9]+q, %r[0-9]+}}
; CHECK64:       jmpr %r7
  %sum = add i16 %a, %b
  %ext = sext i16 %sum to i32
  ret i32 %ext
}

define i32 @add_i16_zext_i32(i16 %a, i16 %b) {
; CHECK64-LABEL: add_i16_zext_i32:
; CHECK64:       {{add %r[0-9]+, %r[0-9]+}}
; CHECK64:       {{movz %r[0-9]+, %r[0-9]+}}
; CHECK64:       {{movz %r[0-9]+q, %r[0-9]+}}
; CHECK64:       jmpr %r7
  %sum = add i16 %a, %b
  %ext = zext i16 %sum to i32
  ret i32 %ext
}
