; RUN: llc -march=etca -mcpu=generic -mattr=+byte < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32,+dw -mattr=+byte < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64,+dw,+qw -mattr=+byte < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64,+dw,+qw -mattr=+byte < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32,+dw,+qw -mattr=+byte < %s | FileCheck %s
; REQUIRES: etca-registered-target

; Test 8-bit (BYTE extension) code generation.
; The backend selects SS=00 instruction variants when operating on i8 types.
; NOTE: The asm printer uses %rNh register names for byte-width instructions.

define signext i8 @add_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: add_byte:
; CHECK:       add %r0h, %r1h
; CHECK:       jmpr %r7
entry:
  %add = add i8 %a, %b
  ret i8 %add
}

define signext i8 @sub_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: sub_byte:
; CHECK:       sub %r0h, %r1h
; CHECK:       jmpr %r7
entry:
  %sub = sub i8 %a, %b
  ret i8 %sub
}

define signext i8 @and_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: and_byte:
; CHECK:       and %r0h, %r1h
; CHECK:       jmpr %r7
entry:
  %and = and i8 %a, %b
  ret i8 %and
}

define signext i8 @or_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: or_byte:
; CHECK:       or %r0h, %r1h
; CHECK:       jmpr %r7
entry:
  %or = or i8 %a, %b
  ret i8 %or
}

define signext i8 @xor_byte(i8 signext %a, i8 signext %b) {
; CHECK-LABEL: xor_byte:
; CHECK:       xor %r0h, %r1h
; CHECK:       jmpr %r7
entry:
  %xor = xor i8 %a, %b
  ret i8 %xor
}

define zeroext i8 @load_byte(i8* %ptr) {
; CHECK-LABEL: load_byte:
; CHECK:       load %r0h, %r0h
; CHECK:       jmpr %r7
entry:
  %val = load i8, i8* %ptr
  ret i8 %val
}

define void @store_byte(i8* %ptr, i8 zeroext %val) {
; CHECK-LABEL: store_byte:
; CHECK:       store %r1h
; CHECK:       jmpr %r7
entry:
  store i8 %val, i8* %ptr
  ret void
}
