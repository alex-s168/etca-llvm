; RUN: llc -mtriple=etca-unknown-elf -mcpu=generic -mattr=+byte < %s 2>&1 | FileCheck %s

; Test G_ABS, G_UADDSAT, G_USUBSAT on 8-bit (byte extension).

declare i8 @llvm.abs.i8(i8, i1)
declare i8 @llvm.uadd.sat.i8(i8, i8)
declare i8 @llvm.usub.sat.i8(i8, i8)

; G_ABS: ashr by 7 (sign bit), add, xor, movs
define i8 @abs_i8(i8 %a) {
; CHECK-LABEL: abs_i8:
; CHECK: call __ashrhi3
; CHECK: add
; CHECK: xor
; CHECK: movs
; CHECK: jmpr %r7
  %r = call i8 @llvm.abs.i8(i8 %a, i1 false)
  ret i8 %r
}

; G_UADDSAT: xor(~a, b), umin, add, movs
define i8 @uadd_sat_i8(i8 %a, i8 %b) {
; CHECK-LABEL: uadd_sat_i8:
; CHECK: xor
; CHECK: cmp
; CHECK: bltu
; CHECK: add
; CHECK: movs
; CHECK: jmpr %r7
  %r = call i8 @llvm.uadd.sat.i8(i8 %a, i8 %b)
  ret i8 %r
}

; G_USUBSAT: cmp, umin, sub, movs
define i8 @usub_sat_i8(i8 %a, i8 %b) {
; CHECK-LABEL: usub_sat_i8:
; CHECK: cmp
; CHECK: bltu
; CHECK: sub
; CHECK: movs
; CHECK: jmpr %r7
  %r = call i8 @llvm.usub.sat.i8(i8 %a, i8 %b)
  ret i8 %r
}
