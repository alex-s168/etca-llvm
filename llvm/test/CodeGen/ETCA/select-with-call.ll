; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 < %s | FileCheck %s --check-prefix=DW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s --check-prefix=QW

;; ===========================================================================
;; ETCA Select + Call Codegen Regression Test
;;
;; Tests that G_SELECT followed by a function call (with stack pointer usage)
;; correctly compiles without machine verifier errors in wider (32/64-bit)
;; modes.
;;
;; Three regressions exposed by this test:
;;
;;   1. ICMP_Pseudo marker was left behind after ISel with GPR32 input
;;      operands but defined with GPR (16-bit) register class, causing:
;;        "Illegal virtual register for instruction - Expected GPR, got GPR32"
;;      Fix: erase consumed ICMP_Pseudo in G_BRCOND handler (Case 1),
;;      and clean up any surviving ICMP_Pseudo in ETCASelectExpand pass.
;;
;;   2. New basic blocks created by ETCASelectExpand (FalseBB, TrueBB,
;;      MBBCont) lacked $r6 (SP) as a live-in register, causing:
;;        "The register $r6 needs to be live in to %bb.N, but is missing
;;         from the live-in list"
;;      Fix: addLiveIn(ETCA::R6) to all three new blocks.
;;
;;   3. G_SELECT consuming G_ICMP via ICMP_Pseudo worked in 16-bit mode
;;      (where GPR == GPR for all operands) but silently broke in 32-bit
;;      and 64-bit modes.  This test verifies both wider sizes.
;;
;; The test uses a select where both arms are the same constant (0),
;; then stores the result and calls an external function.  The select
;; is expanded by ETCASelectExpand into branches, and the call after
;; the select exercises the $r6 live-in propagation.
;; ===========================================================================

target datalayout = "e-m:e-p:32:32-i8:8-i16:16-i32:32-i64:32-f32:32-f64:32-a:0-n8:16:32-S32"
target triple = "etca-unknown-elf32"

define i32 @AM_getIslope(ptr %dx, i32 %0) {
; DW-LABEL: AM_getIslope:
; DW:       push %r5
; DW:       movz %r5, %r6
; DW:       movz %r2d, 0
; DW:       cmp %r1d, %r2d
; DW:       blt .LBB0_2
; DW:       br .LBB0_1
; DW:       store %r1d, %r0d
; DW-NEXT:  call FixedDiv
; DW-NEXT:  movz %r6, %r5
; DW-NEXT:  pop %r5
; DW-NEXT:  jmpr %r7
;
; QW-LABEL: AM_getIslope:
; QW:       push %r5
; QW:       movz %r5, %r6
; QW:       store %r1d, %r0q
; QW:       call FixedDiv
; QW:       jmpr %r7
entry:
  %cmp = icmp slt i32 %0, 0
  %cond = select i1 %cmp, i32 0, i32 0
  store i32 %cond, ptr %dx, align 4
  %call12 = call i32 @FixedDiv()
  ret i32 %call12
}

declare i32 @FixedDiv()
