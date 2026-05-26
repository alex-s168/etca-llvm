; RUN: llc -mtriple=etca-unknown-elf -mcpu=generic -mattr=+64bit < %s 2>&1 | FileCheck %s

; Test G_ABS, G_UADDSAT, G_USUBSAT, G_SADDSAT, G_SSUBSAT on 64-bit word.

declare i64 @llvm.abs.i64(i64, i1)
declare i64 @llvm.uadd.sat.i64(i64, i64)
declare i64 @llvm.usub.sat.i64(i64, i64)
declare i64 @llvm.sadd.sat.i64(i64, i64)
declare i64 @llvm.ssub.sat.i64(i64, i64)

; G_ABS: ashr by 63 (sign bit), add, xor
define i64 @abs_i64(i64 %a) {
; CHECK-LABEL: abs_i64:
; CHECK: add
; CHECK: add
; CHECK: add
; CHECK: add
; CHECK: add
; CHECK: call __ashrdi3
; CHECK: add
; CHECK: xor
; CHECK: jmpr %r7
  %r = call i64 @llvm.abs.i64(i64 %a, i1 false)
  ret i64 %r
}

; G_UADDSAT: xor(~a, b), umin, add
define i64 @uadd_sat_i64(i64 %a, i64 %b) {
; CHECK-LABEL: uadd_sat_i64:
; CHECK: xor
; CHECK: cmp
; CHECK: bltu
; CHECK: add
; CHECK: jmpr %r7
  %r = call i64 @llvm.uadd.sat.i64(i64 %a, i64 %b)
  ret i64 %r
}

; G_USUBSAT: cmp, umin, sub
define i64 @usub_sat_i64(i64 %a, i64 %b) {
; CHECK-LABEL: usub_sat_i64:
; CHECK: cmp
; CHECK: bltu
; CHECK: sub
; CHECK: jmpr %r7
  %r = call i64 @llvm.usub.sat.i64(i64 %a, i64 %b)
  ret i64 %r
}

; G_SADDSAT: MinMax expansion
define i64 @sadd_sat_i64(i64 %a, i64 %b) {
; CHECK-LABEL: sadd_sat_i64:
; CHECK: cmp
; CHECK: sub
; CHECK: cmp
; CHECK: sub
; CHECK: add
; CHECK: jmpr %r7
  %r = call i64 @llvm.sadd.sat.i64(i64 %a, i64 %b)
  ret i64 %r
}

; G_SSUBSAT: MinMax expansion
define i64 @ssub_sat_i64(i64 %a, i64 %b) {
; CHECK-LABEL: ssub_sat_i64:
; CHECK: cmp
; CHECK: sub
; CHECK: cmp
; CHECK: sub
; CHECK: sub
; CHECK: jmpr %r7
  %r = call i64 @llvm.ssub.sat.i64(i64 %a, i64 %b)
  ret i64 %r
}
