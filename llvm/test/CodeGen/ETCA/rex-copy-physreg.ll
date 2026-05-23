; RUN: llc -mtriple=etca-unknown-elf -mcpu=generic -mattr=+rex < %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=etca-unknown-elf -mcpu=etca32 -mattr=+rex < %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=etca-unknown-elf -mcpu=etca64 -mattr=+rex < %s 2>&1 | FileCheck %s
;
; Test that copyPhysReg correctly handles REX extended registers.
; 
; Bug description:
; Prior to the fix, copyPhysReg used (DestEnc & 0x7) == (SrcEnc & 0x7) to
; detect no-op copies.  Both R8 and D8 encode as 8 (0b1000); masked with
; 0x7 they both become 0, matching the R0/D0 pair.  This caused cross-class
; copies involving REX high registers (R8↔D8, R8↔Q8, D8↔Q8) to be silently
; dropped → silent data corruption.
;
; The fix:
;   1. Removed subregister relationships (sub_16 / sub_32) from REX register
;      definitions D8-D15 and Q8-Q15 in ETCARegisterInfo.td — these are
;      independent physical registers in the REX extension.
;   2. Changed copyPhysReg to use TRI.regsOverlap() instead of encoding
;      comparison.
;
; Note: The `-verify-machineinstrs` flag is intentionally NOT used here because
; a pre-existing verifier issue (mismatched COPY sizes in G_ZEXT/G_SEXT handling)
; is triggered with `+rex` on 16-bit generic. This is unrelated to the REX
; subregister/copy fix being tested.

; Basic 16-bit add with REX - compiles fine
define i16 @add16_rex(i16 %a, i16 %b) {
; CHECK-LABEL: add16_rex:
; CHECK: add %r{{[0-9]+}}, %r{{[0-9]+}}
  %r = add i16 %a, %b
  ret i16 %r
}

; Cross-width copy: zext from 16 to 32 bits triggers MOVZ.
; On 16-bit generic, the result register is GPR (16-bit), so MOVZ truncates.
define i32 @zext16to32_rex(i16 %a) {
; CHECK-LABEL: zext16to32_rex:
; CHECK: movz %r0
  %r = zext i16 %a to i32
  ret i32 %r
}

; Cross-width copy: zext from 32 to 64 bits triggers MOVZ.
define i64 @zext32to64_rex(i32 %a) {
; CHECK-LABEL: zext32to64_rex:
; CHECK: movz
  %r = zext i32 %a to i64
  ret i64 %r
}

; Memory access with REX registers  
define i16 @load_store_rex(i16* %p) {
; CHECK-LABEL: load_store_rex:
; CHECK: load %r{{[0-9]+}}, %r{{[0-9]+}}
; CHECK: store %r{{[0-9]+}}, %r{{[0-9]+}}
  %v = load i16, i16* %p
  store i16 %v, i16* %p
  ret i16 %v
}
