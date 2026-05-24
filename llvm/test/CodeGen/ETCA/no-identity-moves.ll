; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s --check-prefix=QW
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 < %s | FileCheck %s --check-prefix=DW

;; ===========================================================================
;; ETCA Identity Move Elimination Test
;;
;; Verifies that the code generator does not emit identity copies of the
;; form "movz %rXq, %rXq" (same qword register as both dst and src) in
;; 64-bit mode.  These are full-width no-op copies that waste code size
;; and add noise to codegen.
;;
;; Identity copies arise from:
;;   1. G_TRUNC + COPY: temp vreg and source vreg may land on same phys reg
;;   2. G_ZEXT/G_SEXT + COPY: same pattern
;;   3. SELECT_Pseudo expansion with identical true/false values
;;   4. copyPhysReg: falling through when DestReg == SrcReg
;;
;; A post-RA pass (ETCAEliminateIdentityMoves) removes MOVZ/S where the
;; operand width matches the register width and dst==src.  Narrower-width
;; MOVZ/S (e.g. MOVS32 in QW mode) are preserved because they extend the
;; value to the full register width, affecting high bits.
;;
;; NOTE: In QW mode, the "movz %r5, %r6" and "movz %r6, %r5" instructions
;; are NOT identity copies (they move between different 16-bit registers
;; in the prologue/epilogue).  Only q-register movz with dst==src are
;; eliminated.
;; ===========================================================================

target datalayout = "e-m:e-p:64:64-i8:8-i16:16-i32:32-i64:64-f32:32-f64:64-a:0-n8:16:32:64-S64"
target triple = "etca-unknown-elf64"

define i32 @AM_getIslope(ptr %dx, i32 %0) {
; QW-LABEL: AM_getIslope:
; QW:       push %r5
; QW:       movz %r5, %r6
; QW:       movz %r1d, 0
; QW:       cmp %r2d, %r1d
; QW:       blt .LBB0_2
; QW:       br .LBB0_1
; QW:       store %r1d, %r0q
; QW:       call FixedDiv
; QW:       movs %r0d, %r0d
; QW:       movz %r6, %r5
; QW:       pop %r5
; QW:       jmpr %r7
;; No identity q-register movz anywhere in the function.
; QW-NOT:   movz %r{{[0-9]+}}q, %r{{[0-9]+}}q
;
; DW-LABEL: AM_getIslope:
; DW:       push %r5
; DW:       movz %r5, %r6
; DW:       movz %r2d, 0
; DW:       cmp %r1d, %r2d
; DW:       blt .LBB0_2
; DW:       br .LBB0_1
; DW:       store %r1d, %r0d
; DW:       call FixedDiv
; DW:       movz %r6, %r5
; DW:       pop %r5
; DW:       jmpr %r7
entry:
  %cmp = icmp slt i32 %0, 0
  %cond = select i1 %cmp, i32 0, i32 0
  store i32 %cond, ptr %dx, align 4
  %call12 = call i32 @FixedDiv()
  ret i32 %call12
}

declare i32 @FixedDiv()
