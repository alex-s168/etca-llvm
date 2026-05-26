; RUN: llc -mtriple=etca-unknown-elf -mcpu=generic < %s 2>&1 | FileCheck %s

; Test G_ABS, G_UADDSAT, G_USUBSAT, G_SADDSAT, G_SSUBSAT on 16-bit word.

declare i16 @llvm.abs.i16(i16, i1)
declare i16 @llvm.uadd.sat.i16(i16, i16)
declare i16 @llvm.usub.sat.i16(i16, i16)
declare i16 @llvm.sadd.sat.i16(i16, i16)
declare i16 @llvm.ssub.sat.i16(i16, i16)

; G_ABS: ashr by 15 (sign bit), add, xor
define i16 @abs_i16(i16 %a) {
; CHECK-LABEL: abs_i16:
; CHECK: call __ashrhi3
; CHECK-NEXT: add
; CHECK-NEXT: xor
; CHECK: jmpr %r7
  %r = call i16 @llvm.abs.i16(i16 %a, i1 false)
  ret i16 %r
}

; G_UADDSAT: xor(~a, b), umin, add
define i16 @uadd_sat_i16(i16 %a, i16 %b) {
; CHECK-LABEL: uadd_sat_i16:
; CHECK: xor
; CHECK: cmp
; CHECK: bltu
; CHECK: add %r0, %r1
; CHECK: jmpr %r7
  %r = call i16 @llvm.uadd.sat.i16(i16 %a, i16 %b)
  ret i16 %r
}

; G_USUBSAT: cmp, umin, sub
define i16 @usub_sat_i16(i16 %a, i16 %b) {
; CHECK-LABEL: usub_sat_i16:
; CHECK: cmp
; CHECK: bltu
; CHECK: sub %r0, %r1
; CHECK: jmpr %r7
  %r = call i16 @llvm.usub.sat.i16(i16 %a, i16 %b)
  ret i16 %r
}

; G_SADDSAT: MinMax expansion (smin + smax + sub + add)
define i16 @sadd_sat_i16(i16 %a, i16 %b) {
; CHECK-LABEL: sadd_sat_i16:
; CHECK: slo
; CHECK: sub
; CHECK: sub
; CHECK: cmp
; CHECK: add
; CHECK: jmpr %r7
  %r = call i16 @llvm.sadd.sat.i16(i16 %a, i16 %b)
  ret i16 %r
}

; G_SSUBSAT: MinMax expansion
define i16 @ssub_sat_i16(i16 %a, i16 %b) {
; CHECK-LABEL: ssub_sat_i16:
; CHECK: slo
; CHECK: sub
; CHECK: sub
; CHECK: cmp
; CHECK: sub
; CHECK: jmpr %r7
  %r = call i16 @llvm.ssub.sat.i16(i16 %a, i16 %b)
  ret i16 %r
}
