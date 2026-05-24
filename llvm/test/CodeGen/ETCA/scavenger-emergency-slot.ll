; RUN: llc -march=etca -mcpu=generic -mattr=+ptr32,+32bit < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32 < %s | FileCheck %s

;; ===========================================================================
;; Register scavenger emergency spill slot test
;;
;; This test verifies that frame index elimination does not crash when
;; ALL registers are in use and a scratch register is needed for STORE
;; address computation.  The function uses many parameters to pressure
;; all available registers, then performs stores to argument pointers
;; and a function call that forces spills from callee-saved registers.
;;
;; ETCa uses the dedicated scratch register R7/D7/Q7 (reserved) for
;; STORE address computation in eliminateFrameIndex, avoiding recursive
;; scavenger spills.
;; ===========================================================================

declare i32 @helper(i32)

; CHECK-LABEL: reg_pressure_32:
; CHECK:       push %r5
; CHECK:       movz %r5, %r6
; CHECK:       jmpr %r7

define i32 @reg_pressure_32(ptr %p1, ptr %p2, ptr %p3, i32 %v1, ptr %p4,
                             ptr %p5, ptr %p6, i32 %v2) {
entry:
  store i32 0, ptr %p1, align 4
  store i32 %v1, ptr %p2, align 4
  %t = load i32, ptr %p3, align 4
  %cmp = icmp eq i32 %t, 0
  br i1 %cmp, label %then, label %else

then:
  store i32 %v2, ptr %p4, align 4
  store i32 0, ptr %p4, align 4
  %call = call i32 @helper(i32 0)
  br label %merge

else:
  store i32 1, ptr %p5, align 4
  br label %merge

merge:
  ret i32 0
}
