; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32,+dw < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64,+dw,+qw < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32,+dw,+qw < %s | FileCheck %s

;; ===========================================================================
;; ETCA Addressing Mode Regression Tests
;;
;; ETCA LOAD/STORE instructions support only [reg] addressing — there is NO
;; immediate offset field in the encoding. isLegalAddressingMode must return
;; true only when BaseOffs == 0.
;;
;; Key invariant: Every load/store instruction takes a single register as its
;; address operand:  "load %rd, %rs" or "store %rv, %ra".
;; There is NEVER "load %rd, %rs+N" or "store %rv, %ra+N".
;;
;; Non-zero offsets are materialized via a separate ADDI before the access.
;;
;; Earlier versions of isLegalAddressingMode returned true for offsets in
;; [-16, 15], which misled LSR into thinking offset addressing was free.
;; ===========================================================================

; ---
; Load with non-zero offset — must use ADDI then pure-register LOAD.
; ---
define i16 @load_with_offset(ptr %p) {
; CHECK-LABEL: load_with_offset:
; Offset 4*2=8 materialized via ADDI, not folded into LOAD.
; CHECK:       add %r{{[0-9]+[dq]?}}, 8
; CHECK-NEXT:  load %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
  %p2 = getelementptr i16, ptr %p, i16 4
  %val = load i16, ptr %p2
  ret i16 %val
}

; ---
; Store with negative offset — must use ADDI then pure-register STORE.
; ---
define void @store_with_offset(ptr %p, i16 %v) {
; CHECK-LABEL: store_with_offset:
; Offset -2*2=-4 materialized via ADDI, not folded into STORE.
; CHECK:       add %r{{[0-9]+[dq]?}}, -4
; CHECK-NEXT:  store %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
  %p2 = getelementptr i16, ptr %p, i16 -2
  store i16 %v, ptr %p2
  ret void
}

; ---
; Loop with array access — LSR might try base+offset addressing.
; Verify loads use pure register operands.
; ---
define i16 @array_loop_sum(ptr %arr, i16 %n) {
; CHECK-LABEL: array_loop_sum:
entry:
  br label %loop

loop:
  %i = phi i16 [0, %entry], [%inext, %loop]
  %sum = phi i16 [0, %entry], [%sum1, %loop]
  %p1 = getelementptr i16, ptr %arr, i16 %i
  %v1 = load i16, ptr %p1
  %sum1 = add i16 %sum, %v1
  %inext = add i16 %i, 1
  %cmp = icmp ult i16 %inext, %n
  br i1 %cmp, label %loop, label %exit

exit:
  ret i16 %sum1
}

; ---
; Adjacent loads from a structure pointer — no offset-based addressing.
; ---
define i16 @struct_access(ptr %s) {
; CHECK-LABEL: struct_access:
; load field 0 at [reg]
; CHECK:       load %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
; Offset 2 computed via ADDI before next load
; CHECK:       add %r{{[0-9]+[dq]?}}, 2
; CHECK-NEXT:  load %r{{[0-9]+[dq]?}}, %r{{[0-9]+[dq]?}}
entry:
  %f0 = load i16, ptr %s
  %p1 = getelementptr i16, ptr %s, i16 1
  %f1 = load i16, ptr %p1
  %sum = add i16 %f0, %f1
  ret i16 %sum
}

; ---
; Store with computed index (requires runtime mul) — store uses pure register.
; ---
define void @array_store(ptr %arr, i16 %idx, i16 %val) {
; CHECK-LABEL: array_store:
; The store uses a pure register operand for the address.
; CHECK:       store %r{{[0-9]+}}, %r0
  %ptr = getelementptr i16, ptr %arr, i16 %idx
  store i16 %val, ptr %ptr
  ret void
}
