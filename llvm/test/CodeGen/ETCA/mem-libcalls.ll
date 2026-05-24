; RUN: llc -march=etca -mcpu=generic -stop-after=legalizer < %s | FileCheck %s --check-prefix=GEN-MIR
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 -stop-after=legalizer < %s | FileCheck %s --check-prefix=DW-MIR
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 -stop-after=legalizer < %s | FileCheck %s --check-prefix=QW-MIR
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32 -stop-after=legalizer < %s | FileCheck %s --check-prefix=QW-MIR
; RUN: llc -march=etca -mcpu=generic -mattr=+byte -stop-after=legalizer < %s | FileCheck %s --check-prefix=GEN-MIR

; RUN: llc -march=etca -mcpu=generic < %s | FileCheck %s --check-prefix=ASM-GEN
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 < %s | FileCheck %s --check-prefix=ASM-DW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 < %s | FileCheck %s --check-prefix=ASM-QW
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr32 < %s | FileCheck %s --check-prefix=ASM-QW
; RUN: llc -march=etca -mcpu=generic -mattr=+byte < %s | FileCheck %s --check-prefix=ASM-GEN

;; ===========================================================================
;; ETCA memory intrinsic libcall tests (memcpy, memmove, memset)
;;
;; Tests that G_MEMCPY, G_MEMMOVE, and G_MEMSET are correctly lowered to
;; calls to the standard C library functions by the legalizer.  Verified
;; at MIR level (CALL_Pseudo &<funcname>) and at final assembly (call).
;;
;; All word/address size combinations are tested:
;;   GEN  = 16-bit word + 16-bit pointer (generic default)
;;   DW   = 32-bit word + 32-bit pointer
;;   QW   = 64-bit word + 64-bit pointer / 64-bit word + 32-bit pointer
;; ===========================================================================

; --- memcpy ---

define void @test_memcpy(ptr %dst, ptr %src) {
; GEN-MIR-LABEL: name: test_memcpy
; GEN-MIR: CALL_Pseudo &memcpy,
; DW-MIR-LABEL: name: test_memcpy
; DW-MIR: CALL_Pseudo &memcpy,
; QW-MIR-LABEL: name: test_memcpy
; QW-MIR: CALL_Pseudo &memcpy,
;
; ASM-GEN-LABEL: test_memcpy:
; ASM-GEN:       call memcpy
; ASM-DW-LABEL: test_memcpy:
; ASM-DW:       call memcpy
; ASM-QW-LABEL: test_memcpy:
; ASM-QW:       call memcpy
  call void @llvm.memcpy.p0.p0.i32(ptr %dst, ptr %src, i32 16, i1 false)
  ret void
}

; --- memmove ---

define void @test_memmove(ptr %dst, ptr %src) {
; GEN-MIR-LABEL: name: test_memmove
; GEN-MIR: CALL_Pseudo &memmove,
; DW-MIR-LABEL: name: test_memmove
; DW-MIR: CALL_Pseudo &memmove,
; QW-MIR-LABEL: name: test_memmove
; QW-MIR: CALL_Pseudo &memmove,
;
; ASM-GEN-LABEL: test_memmove:
; ASM-GEN:       call memmove
; ASM-DW-LABEL: test_memmove:
; ASM-DW:       call memmove
; ASM-QW-LABEL: test_memmove:
; ASM-QW:       call memmove
  call void @llvm.memmove.p0.p0.i32(ptr %dst, ptr %src, i32 16, i1 false)
  ret void
}

; --- memset ---

define void @test_memset(ptr %dst) {
; GEN-MIR-LABEL: name: test_memset
; GEN-MIR: CALL_Pseudo &memset,
; DW-MIR-LABEL: name: test_memset
; DW-MIR: CALL_Pseudo &memset,
; QW-MIR-LABEL: name: test_memset
; QW-MIR: CALL_Pseudo &memset,
;
; ASM-GEN-LABEL: test_memset:
; ASM-GEN:       call memset
; ASM-DW-LABEL: test_memset:
; ASM-DW:       call memset
; ASM-QW-LABEL: test_memset:
; ASM-QW:       call memset
  call void @llvm.memset.p0.i32(ptr %dst, i8 0, i32 16, i1 false)
  ret void
}

; --- memcpy with i64 size (used by 64-bit word targets) ---

define void @test_memcpy_i64(ptr %dst, ptr %src) {
; GEN-MIR-LABEL: name: test_memcpy_i64
; GEN-MIR: CALL_Pseudo &memcpy,
; DW-MIR-LABEL: name: test_memcpy_i64
; DW-MIR: CALL_Pseudo &memcpy,
; QW-MIR-LABEL: name: test_memcpy_i64
; QW-MIR: CALL_Pseudo &memcpy,
;
; ASM-GEN-LABEL: test_memcpy_i64:
; ASM-GEN:       call memcpy
; ASM-DW-LABEL: test_memcpy_i64:
; ASM-DW:       call memcpy
; ASM-QW-LABEL: test_memcpy_i64:
; ASM-QW:       call memcpy
  call void @llvm.memcpy.p0.p0.i64(ptr %dst, ptr %src, i64 16, i1 false)
  ret void
}

; --- memcpy with i16 size ---

define void @test_memcpy_i16(ptr %dst, ptr %src) {
; GEN-MIR-LABEL: name: test_memcpy_i16
; GEN-MIR: CALL_Pseudo &memcpy,
; DW-MIR-LABEL: name: test_memcpy_i16
; DW-MIR: CALL_Pseudo &memcpy,
; QW-MIR-LABEL: name: test_memcpy_i16
; QW-MIR: CALL_Pseudo &memcpy,
;
; ASM-GEN-LABEL: test_memcpy_i16:
; ASM-GEN:       call memcpy
; ASM-DW-LABEL: test_memcpy_i16:
; ASM-DW:       call memcpy
; ASM-QW-LABEL: test_memcpy_i16:
; ASM-QW:       call memcpy
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 8, i1 false)
  ret void
}

declare void @llvm.memcpy.p0.p0.i32(ptr, ptr, i32, i1)
declare void @llvm.memmove.p0.p0.i32(ptr, ptr, i32, i1)
declare void @llvm.memset.p0.i32(ptr, i8, i32, i1)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memcpy.p0.p0.i16(ptr, ptr, i16, i1)
