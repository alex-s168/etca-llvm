; RUN: llc -mtriple=etca-unknown-elf -mattr=+saf -global-isel -stop-after=legalizer \
; RUN:   < %s 2>&1 | FileCheck %s --check-prefix=LEGAL

; Test that G_VASTART and G_VAARG work correctly for a variadic function
; with a single named argument (3 unallocated arg regs → 6 byte save area).

declare void @llvm.va_start(ptr)

define i16 @get_first(i16 %n, ...) {
; LEGAL-LABEL: name: get_first

; Varargs save area: regular stack object, size 6 = 3 regs × 2 bytes.
; LEGAL:      stack:
; LEGAL:        - { id: 0, name: '', type: default, offset: 0, size: 6, alignment: 2

; G_VASTART stores address of stack.0 into the alloca'd ptr (stack.1.ap).
; LEGAL:      [[AP:%[0-9]+]]:_(p0) = G_FRAME_INDEX %stack.1.ap
; LEGAL:      [[BASE:%[0-9]+]]:_(p0) = G_FRAME_INDEX %stack.0
; LEGAL:      G_STORE [[BASE]](p0), [[AP]](p0) :: (store (s16) into %ir.ap)

; G_VAARG expands to: load ap, advance by 2, store ap, load value.
; LEGAL:      [[CUR:%[0-9]+]]:_(p0) = G_LOAD [[AP]](p0) :: (load (p0))
; LEGAL:      [[INC:%[0-9]+]]:_(s16) = G_CONSTANT i16 2
; LEGAL:      [[NEXT:%[0-9]+]]:_(p0) = G_PTR_ADD [[CUR]], [[INC]](s16)
; LEGAL:      G_STORE [[NEXT]](p0), [[AP]](p0) :: (store (p0))
; LEGAL:      [[VAL:%[0-9]+]]:_(s16) = G_LOAD [[CUR]](p0) :: (load (s16))
entry:
  %ap = alloca ptr, align 2
  call void @llvm.va_start(ptr %ap)
  %v = va_arg ptr %ap, i16
  ret i16 %v
}
