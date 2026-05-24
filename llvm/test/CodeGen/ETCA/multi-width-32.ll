; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 < %s | FileCheck %s --check-prefix=DW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s --check-prefix=QW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s --check-prefix=QW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32 < %s | FileCheck %s --check-prefix=QW

;; ===========================================================================
;; ETCA 32-bit arithmetic — DW and QW modes
;; ===========================================================================

define i32 @add_i32(i32 %a, i32 %b) {
; DW-LABEL: add_i32:
; DW:       {{add %r[0-9]+d, %r[0-9]+d}}
; DW:       jmpr %r7
;
; QW-LABEL: add_i32:
; QW:       {{add %r[0-9]+d, %r[0-9]+d}}
; QW:       jmpr %r7
  %r = add i32 %a, %b
  ret i32 %r
}

define i32 @and_i32(i32 %a, i32 %b) {
; DW-LABEL: and_i32:
; DW:       {{and %r[0-9]+d, %r[0-9]+d}}
; DW:       jmpr %r7
;
; QW-LABEL: and_i32:
; QW:       {{and %r[0-9]+d, %r[0-9]+d}}
; QW:       jmpr %r7
  %r = and i32 %a, %b
  ret i32 %r
}
