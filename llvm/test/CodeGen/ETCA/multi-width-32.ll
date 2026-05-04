; RUN: llc -march=etca -mcpu=etca32 < %s | FileCheck %s --check-prefix=CHECK32
; RUN: llc -march=etca -mcpu=etca64 < %s | FileCheck %s --check-prefix=CHECK64

;; ===========================================================================
;; ETCA 32-bit arithmetic tests — CPUs with DW extension
;;
;; Tests that 32-bit arithmetic works on DW-capable CPUs (etca32, etca64).
;; On etca32 (32-bit native), 32-bit ops use direct register names (rNd).
;; On etca64 (64-bit native), 32-bit ops need MOVZ bridging (rNd → rNq).
;; ===========================================================================

define i32 @add_i32(i32 %a, i32 %b) {
; CHECK32-LABEL: add_i32:
; CHECK32:       add %r0d, %r1d
; CHECK32:       jmpr %r7
;
; CHECK64-LABEL: add_i32:
; CHECK64:       movz %r0d, %r0q
; CHECK64:       movz %r1d, %r1q
; CHECK64:       add %r0d, %r1d
; CHECK64:       movz %r0q, %r0d
; CHECK64:       jmpr %r7
  %r = add i32 %a, %b
  ret i32 %r
}

define i32 @and_i32(i32 %a, i32 %b) {
; CHECK32-LABEL: and_i32:
; CHECK32:       and %r0d, %r1d
; CHECK32:       jmpr %r7
;
; CHECK64-LABEL: and_i32:
; CHECK64:       movz %r0d, %r0q
; CHECK64:       movz %r1d, %r1q
; CHECK64:       and %r0d, %r1d
; CHECK64:       movz %r0q, %r0d
; CHECK64:       jmpr %r7
  %r = and i32 %a, %b
  ret i32 %r
}
