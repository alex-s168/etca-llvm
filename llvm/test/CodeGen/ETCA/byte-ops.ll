; RUN: llc -march=etca -mcpu=generic -mattr=+byte < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 -mattr=+byte < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 -mattr=+byte < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 -mattr=+byte < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32 -mattr=+byte < %s | FileCheck %s
; REQUIRES: etca-registered-target

; Test 8-bit (BYTE extension) code generation.
; Uses regex {{...}} for register names to match all width suffixes.

define signext i8 @add_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: add_byte:
; CHECK:       {{add %r[0-9]+h, %r[0-9]+h}}
; CHECK:       jmpr %r7
entry:
  %add = add i8 %a, %b
  ret i8 %add
}

define signext i8 @sub_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: sub_byte:
; CHECK:       {{sub %r[0-9]+h, %r[0-9]+h}}
; CHECK:       jmpr %r7
entry:
  %sub = sub i8 %a, %b
  ret i8 %sub
}

define signext i8 @and_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: and_byte:
; CHECK:       {{and %r[0-9]+h, %r[0-9]+h}}
; CHECK:       jmpr %r7
entry:
  %and = and i8 %a, %b
  ret i8 %and
}

define signext i8 @or_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: or_byte:
; CHECK:       {{or %r[0-9]+h, %r[0-9]+h}}
; CHECK:       jmpr %r7
entry:
  %or = or i8 %a, %b
  ret i8 %or
}

define signext i8 @xor_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: xor_byte:
; CHECK:       {{xor %r[0-9]+h, %r[0-9]+h}}
; CHECK:       jmpr %r7
entry:
  %xor = xor i8 %a, %b
  ret i8 %xor
}

define zeroext i8 @load_byte(i8* %ptr) {
; CHECK-LABEL: load_byte:
; CHECK:       {{load %r[0-9]+[dqh]?, %r[0-9]+[dqh]?}}
; CHECK:       jmpr %r7
entry:
  %val = load i8, i8* %ptr
  ret i8 %val
}

define void @store_byte(i8* %ptr, i8 zeroext %val) {
; CHECK-LABEL: store_byte:
; CHECK:       {{store %r[0-9]+[dqh]?, %r[0-9]+[dqh]?}}
; CHECK:       jmpr %r7
entry:
  store i8 %val, i8* %ptr
  ret void
}
