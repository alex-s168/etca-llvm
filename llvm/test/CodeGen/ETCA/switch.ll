; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s

; Test that switch statements with multiple cases compile without crashing.
;
; Functions with ≥4 cases may use jump tables (G_JUMP_TABLE + G_BRJT).
; The JTI pseudo is expanded to a MOVZI+SLO chain with R_ETCA_MOV_*
; spanning relocation, resolved by the linker's etca_build_mov_ri.

; CHECK-LABEL: D_DoAdvanceDemo:
; CHECK: jmpr	%r7
; CHECK-LABEL: doSwitch:
; CHECK: jmpr	%r7
; CHECK-LABEL: manyCases:
; CHECK: jmpr	%r7

define void @D_DoAdvanceDemo() #0 {
entry:
  switch i16 0, label %sw.bb [
    i16 0, label %sw.bb
    i16 1, label %sw.bb12
    i16 2, label %sw.bb13
    i16 3, label %sw.bb14
    i16 4, label %sw.bb15
  ]

sw.bb:                                            ; preds = %entry, %entry
  ret void

sw.bb12:                                          ; preds = %entry
  ret void

sw.bb13:                                          ; preds = %entry
  ret void

sw.bb14:                                          ; preds = %entry
  ret void

sw.bb15:                                          ; preds = %entry
  ret void
}

define i16 @doSwitch(i16 %n) #0 {
entry:
  switch i16 %n, label %default [
    i16 0, label %zero
    i16 1, label %one
    i16 2, label %two
    i16 3, label %three
  ]

zero:
  ret i16 100

one:
  ret i16 200

two:
  ret i16 300

three:
  ret i16 400

default:
  ret i16 0
}

define i16 @manyCases(i16 %n) #0 {
entry:
  switch i16 %n, label %default [
    i16 0, label %c0
    i16 1, label %c1
    i16 2, label %c2
    i16 3, label %c3
    i16 4, label %c4
    i16 5, label %c5
    i16 6, label %c6
    i16 7, label %c7
    i16 8, label %c8
    i16 9, label %c9
    i16 10, label %c10
    i16 11, label %c11
  ]

c0:   ret i16 0
c1:   ret i16 1
c2:   ret i16 2
c3:   ret i16 3
c4:   ret i16 4
c5:   ret i16 5
c6:   ret i16 6
c7:   ret i16 7
c8:   ret i16 8
c9:   ret i16 9
c10:  ret i16 10
c11:  ret i16 11
default:
  ret i16 255
}

attributes #0 = { noinline optnone }
