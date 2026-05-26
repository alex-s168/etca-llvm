; RUN: llc -mtriple=etca-unknown-elf -mcpu=generic -mattr=+32bit < %s 2>&1 | FileCheck %s

; Test G_ABS, G_UADDSAT, G_USUBSAT, G_SADDSAT, G_SSUBSAT on 32-bit word.

declare i32 @llvm.abs.i32(i32, i1)
declare i32 @llvm.uadd.sat.i32(i32, i32)
declare i32 @llvm.usub.sat.i32(i32, i32)
declare i32 @llvm.sadd.sat.i32(i32, i32)
declare i32 @llvm.ssub.sat.i32(i32, i32)

; G_ABS: ashr by 31 (sign bit), add, xor
define i32 @abs_i32(i32 %a) {
; CHECK-LABEL: abs_i32:
; CHECK: call __ashrsi3
; CHECK: add
; CHECK: xor
; CHECK: jmpr %r7
  %r = call i32 @llvm.abs.i32(i32 %a, i1 false)
  ret i32 %r
}

; G_UADDSAT: xor(~a, b), umin, add
define i32 @uadd_sat_i32(i32 %a, i32 %b) {
; CHECK-LABEL: uadd_sat_i32:
; CHECK: xor
; CHECK: cmp
; CHECK: bltu
; CHECK: add
; CHECK: jmpr %r7
  %r = call i32 @llvm.uadd.sat.i32(i32 %a, i32 %b)
  ret i32 %r
}

; G_USUBSAT: cmp, umin, sub
define i32 @usub_sat_i32(i32 %a, i32 %b) {
; CHECK-LABEL: usub_sat_i32:
; CHECK: cmp
; CHECK: bltu
; CHECK: sub
; CHECK: jmpr %r7
  %r = call i32 @llvm.usub.sat.i32(i32 %a, i32 %b)
  ret i32 %r
}

; G_SADDSAT: MinMax expansion (smin + smax + sub + add)
define i32 @sadd_sat_i32(i32 %a, i32 %b) {
; CHECK-LABEL: sadd_sat_i32:
; CHECK: cmp
; CHECK: sub
; CHECK: cmp
; CHECK: sub
; CHECK: add
; CHECK: jmpr %r7
  %r = call i32 @llvm.sadd.sat.i32(i32 %a, i32 %b)
  ret i32 %r
}

; G_SSUBSAT: MinMax expansion
define i32 @ssub_sat_i32(i32 %a, i32 %b) {
; CHECK-LABEL: ssub_sat_i32:
; CHECK: cmp
; CHECK: sub
; CHECK: cmp
; CHECK: sub
; CHECK: sub
; CHECK: jmpr %r7
  %r = call i32 @llvm.ssub.sat.i32(i32 %a, i32 %b)
  ret i32 %r
}
