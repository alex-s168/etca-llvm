; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s --check-prefix=CHECK64

;; ===========================================================================
;; ETCA 64-bit arithmetic tests — CPUs with QW extension
;;
;; Tests that 64-bit arithmetic works on QW-capable CPUs (word size >= 64).
;; On these CPUs, 64-bit ops use native register names (rNq).
;; ===========================================================================

define i64 @add_i64(i64 %a, i64 %b) {
; CHECK64-LABEL: add_i64:
; CHECK64:       add %r0q, %r1q
; CHECK64:       jmpr %r7
  %r = add i64 %a, %b
  ret i64 %r
}
