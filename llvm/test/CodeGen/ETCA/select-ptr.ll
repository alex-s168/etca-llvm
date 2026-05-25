; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 -O1 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr64 -O1 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32 -O1 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 -O1 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -O1 < %s | FileCheck %s

;; ===========================================================================
;; Regression test: unable to legalize G_SELECT with p0 (pointer) result type
;;
;; When G_SELECT produces a pointer-typed result (p0), the legalizer only
;; had legalFor entries for scalar types (s8, s16, s32, s64).  Pointer types
;; were not listed, causing:
;;
;;   "LLVM ERROR: unable to legalize instruction:
;;    %1:_(p0) = G_SELECT %3:_(s16), %2:_, %2:_"
;;
;; The fix adds: SelectActions.legalFor({{p0, s16}});
;; p0 size matches the pointer width (16/32/64) and is handled by the
;; instruction selector's G_SELECT handler via getRCForType.
;; ===========================================================================

; CHECK-LABEL: D_SetGameDescription:
; CHECK:      movz %r0
; CHECK:      jmpr %r7

define ptr @D_SetGameDescription() {
entry:
  %.str.36..str.37 = select i1 false, ptr null, ptr null
  ret ptr %.str.36..str.37
}
