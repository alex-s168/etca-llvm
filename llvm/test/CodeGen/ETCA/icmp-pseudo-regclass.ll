; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 -O1 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr64 -O1 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32 -O1 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 -O1 < %s | FileCheck %s

;; ===========================================================================
;; Regression test: VReg has no regclass after selection for ICMP_Pseudo
;;
;; When G_BRCOND is visited before G_ICMP (reverse-order traversal in the
;; instruction selector), G_BRCOND's Case 2 handler creates ICMP_Pseudo
;; to replace G_ICMP, but failed to constrain the destination virtual
;; register to a register class.  Later, G_SELECT's Case 1 handler found
;; the ICMP_Pseudo and used it, but the unconstrained register caused:
;;
;;   "LLVM ERROR: VReg has no regclass after selection:
;;    %X:gprb(s16) = ICMP_Pseudo Y"
;;
;; The test combines G_ICMP + G_SELECT + G_BRCOND + G_STORE, which
;; exercises the reverse-order path through all three handlers.
;;
;; Fix: add constrainReg call in G_BRCOND's Case 2 handler, matching
;; the existing constrainReg in G_SELECT's Case 2 handler.
;; ===========================================================================

; CHECK-LABEL: D_DoAdvanceDemo:
; CHECK:      beq .LBB0_2
; CHECK:      br .LBB0_1
; CHECK:      .LBB0_1:
; CHECK:      br .LBB0_3
; CHECK:      .LBB0_2:
; CHECK:      .LBB0_3:
; CHECK:      beq .LBB0_4
; CHECK:      br .LBB0_5
; CHECK:      .LBB0_4:
; CHECK:      jmpr %r7
; CHECK:      .LBB0_5:
; CHECK:      jmpr %r7

target datalayout = "e-m:e-p:32:32-i8:8-i16:16-i32:32-i64:32-f32:32-f64:32-a:0-n8:16:32-S32"
target triple = "etca-unknown-elf32"

define void @D_DoAdvanceDemo(i32 %0) {
entry:
  %cmp4 = icmp eq i32 %0, 0
  %. = select i1 %cmp4, i32 0, i32 0
  store i32 %., ptr null, align 4
  br i1 %cmp4, label %if.then9, label %if.else10

if.then9:                                         ; preds = %entry
  ret void

if.else10:                                        ; preds = %entry
  ret void
}
