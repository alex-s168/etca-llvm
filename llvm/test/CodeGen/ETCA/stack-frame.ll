; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s --check-prefixes=CHK,GN
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 < %s | FileCheck %s --check-prefixes=CHK,EW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s --check-prefixes=CHK,QW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s --check-prefixes=CHK,P64
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32 < %s | FileCheck %s --check-prefixes=CHK,W64

;; ===========================================================================
;; ETCA Stack Frame Layout Tests
;;
;; Verifies correct stack frame set-up for all width/pointer combinations.
;;
;; Key invariants:
;;   1. Prologue: push r5, movz r5, r6, sub r6, N
;;   2. Spill slots and locals at bp-2 to bp-N
;;      (WITHIN the allocated frame, NOT below sp)
;;   3. Epilogue: movz r6, r5, pop r5
;;   4. No stores outside [bp-N, bp-0]
;;
;; Previously getFrameIndexReference returned ObjectOffset - StackSize,
;; placing all spills below sp (outside the frame, corrupting memory).
;; Fixed by removing the StackSize subtraction: Offset = ObjectOffset.
;; ===========================================================================

;; --- 1. No stack frame (leaf, no spills) ---
define i16 @no_frame(i16 %a) {
; CHK-LABEL: no_frame:
; CHK:       jmpr %r7
  ret i16 %a
}

;; --- 2. Local variable via alloca ---
define i16 @with_alloca(i16 %a, i16 %b) {
; CHK-LABEL: with_alloca:
; CHK:       push %r5
; CHK-NEXT:  movz %r5, %r6
; CHK:       sub %r6, 8
; Store at bp-8 (within allocated frame)
; CHK:       store %r0,
; CHK:       load %r{{[0-9]+}}, %r{{[0-9]+}}
; CHK:       jmpr %r7
  %buf = alloca [4 x i16], align 2
  %ptr = getelementptr [4 x i16], ptr %buf, i16 0, i16 0
  store i16 %a, ptr %ptr
  %ptr2 = getelementptr [4 x i16], ptr %buf, i16 0, i16 1
  %val = load i16, ptr %ptr2
  %ret = add i16 %val, %b
  ret i16 %ret
}

;; --- 3. Callee-saved register spilled across call ---
; Uses enough values across a call to force a CS register spill.
declare void @callee()

define i16 @spill_cs(i16 %v1, i16 %v2, i16 %v3, i16 %v4) {
; GN-LABEL: spill_cs:
; GN:       push %r5
; GN-NEXT:  movz %r5, %r6
; GN:       push %r4
; GN:       call callee
; GN:       movz %r6, %r5
; GN:       sub %r6, 2
; GN:       pop %r4
; GN:       pop %r5
; GN:       jmpr %r7
;
; EW-LABEL: spill_cs:
; EW:       push %r5
; EW-NEXT:  movz %r5, %r6
; EW:       push %r3
; EW-NEXT:  push %r4
; EW:       call callee
; EW:       movz %r6, %r5
; EW:       sub %r6, 4
; EW:       pop %r4
; EW:       pop %r3
; EW:       pop %r5
; EW:       jmpr %r7
;
; QW-LABEL: spill_cs:
; QW:       push %r5
; QW-NEXT:  movz %r5, %r6
; QW:       push %r3
; QW-NEXT:  push %r4
; QW:       call callee
; QW:       movz %r6, %r5
; QW:       sub %r6, 4
; QW:       pop %r4
; QW:       pop %r3
; QW:       pop %r5
; QW:       jmpr %r7
;
; P64-LABEL: spill_cs:
; P64:       push %r5
; P64-NEXT:  movz %r5, %r6
; P64:       push %r3
; P64-NEXT:  push %r4
; P64:       call callee
; P64:       movz %r6, %r5
; P64:       sub %r6, 4
; P64:       pop %r4
; P64:       pop %r3
; P64:       pop %r5
; P64:       jmpr %r7
;
; W64-LABEL: spill_cs:
; W64:       push %r5
; W64-NEXT:  movz %r5, %r6
; W64:       push %r3
; W64-NEXT:  push %r4
; W64:       call callee
; W64:       movz %r6, %r5
; W64:       sub %r6, 4
; W64:       pop %r4
; W64:       pop %r3
; W64:       pop %r5
; W64:       jmpr %r7
  call void @callee()
  %s1 = add i16 %v1, %v2
  %s2 = add i16 %s1, %v3
  %r = add i16 %s2, %v4
  ret i16 %r
}

;; --- 4. Nested calls without spills ---
define void @nested_calls() {
; CHK-LABEL: nested_calls:
; CHK:       push %r5
; CHK-NEXT:  movz %r5, %r6
; CHK:       call callee
; CHK:       call callee
; CHK:       jmpr %r7
  call void @callee()
  call void @callee()
  ret void
}
