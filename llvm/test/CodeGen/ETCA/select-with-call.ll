; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 < %s | FileCheck %s --check-prefix=DW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s --check-prefix=QW

;; ===========================================================================
;; ETCA Select + Call + Empty-block Codegen Regression Test
;;
;; Tests that combine select (via CMP + branch), function calls (via
;; CALL_Pseudo + ADJCALLSTACKDOWN/UP), and empty intermediate basic
;; blocks correctly compile without machine verifier errors.
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
;;      Fix: addLiveIn(ETCA::R6) to all three new blocks, plus a global
;;      loop in ETCASelectExpand that ensures $r6 is live-in to every block.
;;
;;   3. Empty intermediate blocks (created by optnone functions or other
;;      IR that preserves empty blocks) lacked $r6 as a live-in, causing
;;      the same verifier error above, because $r6 must flow through the
;;      CFG from the entry block (where it's live-in) to the call block
;;      (where it's used by ADJCALLSTACKDOWN/UP).
;;      Fix: global $r6 live-in loop in ETCASelectExpand.
;; ===========================================================================

target datalayout = "e-m:e-p:32:32-i8:8-i16:16-i32:32-i64:32-f32:32-f64:32-a:0-n8:16:32-S32"
target triple = "etca-unknown-elf32"

;; --- AM_getIslope (select + store + call) ---
;; Tests that G_SELECT + G_STORE + function call compile correctly.

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
; QW:       movz %r1d, 0
; QW:       cmp %r2d, %r1d
; QW:       blt .LBB0_2
; QW:       br .LBB0_1
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


;; --- AM_optnone (noinline optnone + empty blocks + call) ---
;; Tests that a function with noinline optnone attributes, empty
;; intermediate blocks, and a function call compiles correctly.
;; The empty blocks caused $r6 live-in failures because LLVM's
;; LiveIntervals couldn't propagate $r6 through the empty blocks.

; Function Attrs: noinline optnone
define i32 @AM_optnone() #0 {
; DW-LABEL: AM_optnone:
; DW:       push %r5
; DW:       movz %r5, %r6
; DW:       call FixedDiv
; DW:       movz %r6, %r5
; DW:       pop %r5
; DW:       jmpr %r7
;
; QW-LABEL: AM_optnone:
; QW:       push %r5
; QW:       movz %r5, %r6
; QW:       call FixedDiv
; QW:       movz %r6, %r5
; QW:       pop %r5
; QW:       jmpr %r7
entry:
  br label %if.end

if.end:
  br label %if.else11

if.else11:
  %call12 = call i32 @FixedDiv()
  ret i32 %call12
}

declare i32 @FixedDiv()

attributes #0 = { noinline optnone "target-cpu"="generic" "target-features"="+ptr32,+32bit" }
