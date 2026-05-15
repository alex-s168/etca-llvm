; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca32 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca64 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca32p64 < %s | FileCheck %s
; RUN: llc -march=etca -mcpu=etca64p32 < %s | FileCheck %s

;; ===========================================================================
;; ETCA memory operation codegen tests
;; ===========================================================================

define i16 @load_i16(i16* %ptr) {
; CHECK-LABEL: load_i16:
; CHECK:       load %r0, %r0
; CHECK:       jmpr %r7
  %val = load i16, i16* %ptr
  ret i16 %val
}

define void @store_i16(i16* %ptr, i16 %val) {
; CHECK-LABEL: store_i16:
; CHECK:       store %r1
; CHECK:       jmpr %r7
  store i16 %val, i16* %ptr
  ret void
}

define i16 @load_after_store(i16* %ptr, i16 %val) {
; CHECK-LABEL: load_after_store:
; CHECK:       store %r1
; CHECK:       load %r0
; CHECK:       jmpr %r7
  store i16 %val, i16* %ptr
  %reload = load i16, i16* %ptr
  ret i16 %reload
}

define i16 @stack_alloca(i16 %val) {
; CHECK-LABEL: stack_alloca:
; CHECK:       sub %r6, 2
; CHECK:       movz %r1, %r5
; CHECK:       store %r0, %r1
; CHECK:       load %r0, %r1
; CHECK:       jmpr %r7
  %ptr = alloca i16
  store i16 %val, i16* %ptr
  %reload = load i16, i16* %ptr
  ret i16 %reload
}

define i16 @load_store_different(i16* %in, i16* %out) {
; CHECK-LABEL: load_store_different:
; CHECK:       load %r0, %r0
; CHECK:       store %r0, %r1
; CHECK:       jmpr %r7
  %val = load i16, i16* %in
  store i16 %val, i16* %out
  ret i16 %val
}

define i16 @volatile_load(i16* %ptr) {
; CHECK-LABEL: volatile_load:
; CHECK:       load %r0, %r0
; CHECK:       jmpr %r7
  %val = load volatile i16, i16* %ptr
  ret i16 %val
}

define void @volatile_store(i16* %ptr, i16 %val) {
; CHECK-LABEL: volatile_store:
; CHECK:       store %r1
; CHECK:       jmpr %r7
  store volatile i16 %val, i16* %ptr
  ret void
}

define void @store_zero(i16* %ptr) {
; CHECK-LABEL: store_zero:
; CHECK:       movz
; CHECK:       store
; CHECK:       jmpr %r7
  store i16 0, i16* %ptr
  ret void
}

define i16 @multi_alloca(i16 %a, i16 %b) {
; CHECK-LABEL: multi_alloca:
; CHECK:       sub %r6
; CHECK:       add %r5
; CHECK:       store %r3, %r5
; CHECK:       add %r5
; CHECK:       movz
; CHECK:       store
; CHECK:       store
; CHECK:       load
; CHECK:       load
; CHECK:       add
; CHECK:       jmpr %r7
  %p1 = alloca i16
  %p2 = alloca i16
  store i16 %a, i16* %p1
  store i16 %b, i16* %p2
  %v1 = load i16, i16* %p1
  %v2 = load i16, i16* %p2
  %sum = add i16 %v1, %v2
  ret i16 %sum
}
