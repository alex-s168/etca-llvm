; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s --check-prefix=ALL
; RUN: llc -march=etca -mcpu=generic -mattr=+ptr32,+32bit < %s | FileCheck %s --check-prefix=ALL
; RUN: llc -march=etca -mcpu=generic -mattr=+ptr64,+64bit < %s | FileCheck %s --check-prefix=ALL

; Test that G_ICMP with pointer operands legalizes and selects correctly.

target datalayout = "e-m:e-p:16:16-i8:8-i16:16-i32:32-i64:32-f32:32-f64:32-a:0-n8:16:32-S16"
target triple = "etca-unknown-elf16"

; ALL-LABEL: D_PtrNotNull:
; ALL: cmp
define i1 @D_PtrNotNull(ptr %call) {
entry:
  %cmp = icmp ne ptr %call, null
  ret i1 %cmp
}

; ALL-LABEL: D_PtrEqBranch:
; ALL: cmp
define void @D_PtrEqBranch(ptr %call) {
entry:
  %cmp = icmp eq ptr %call, null
  br i1 %cmp, label %if.then, label %if.end
if.then:
  store i16 1, ptr %call
  br label %if.end
if.end:
  ret void
}

; ALL-LABEL: D_PtrSelect:
; ALL: cmp
define i16 @D_PtrSelect(ptr %call) {
entry:
  %cmp = icmp ne ptr %call, null
  %res = select i1 %cmp, i16 42, i16 0
  ret i16 %res
}
