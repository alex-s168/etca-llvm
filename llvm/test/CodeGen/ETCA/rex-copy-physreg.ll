; RUN: llc -mtriple=etca-unknown-elf -mcpu=generic -mattr=+rex < %s 2>&1 | FileCheck %s --check-prefix=GEN
; RUN: llc -mtriple=etca-unknown-elf -mcpu=generic -mattr=+32bit,+ptr32,+rex < %s 2>&1 | FileCheck %s --check-prefix=DW
; RUN: llc -mtriple=etca-unknown-elf -mcpu=generic -mattr=+64bit,+ptr64,+rex < %s 2>&1 | FileCheck %s --check-prefix=QW
;
; Test that copyPhysReg correctly handles REX extended registers.

; Basic 16-bit add with REX
define i16 @add16_rex(i16 %a, i16 %b) {
; GEN-LABEL: add16_rex:
; GEN:       {{add %r[0-9]+, %r[0-9]+}}
;
; DW-LABEL: add16_rex:
; DW:       {{add %r[0-9]+, %r[0-9]+}}
;
; QW-LABEL: add16_rex:
; QW:       {{add %r[0-9]+, %r[0-9]+}}
  %r = add i16 %a, %b
  ret i16 %r
}

; Cross-width copy: zext from 16 to 32 bits
define i32 @zext16to32_rex(i16 %a) {
; GEN-LABEL: zext16to32_rex:
; GEN:       {{movz %r[0-9]+, %r[0-9]+}}
;
; DW-LABEL: zext16to32_rex:
; DW:       {{movz %r[0-9]+[dq]?, [0-9]+}}
; DW:       {{and %r[0-9]+d, %r[0-9]+d}}
;
; QW-LABEL: zext16to32_rex:
; QW:       {{and %r[0-9]+q, %r[0-9]+q}}
  %r = zext i16 %a to i32
  ret i32 %r
}

; Cross-width copy: zext from 32 to 64 bits
define i64 @zext32to64_rex(i32 %a) {
; GEN-LABEL: zext32to64_rex:
; GEN:       {{movz %r[0-9]+d, %r[0-9]+d}}
;
; DW-LABEL: zext32to64_rex:
; DW:       {{movz %r[0-9]+d, %r[0-9]+d}}
;
; QW-LABEL: zext32to64_rex:
; QW:       {{and %r[0-9]+q, %r[0-9]+q}}
  %r = zext i32 %a to i64
  ret i64 %r
}

; Memory access with REX registers
define i16 @load_store_rex(i16* %p) {
; GEN-LABEL: load_store_rex:
; GEN:       {{load %r[0-9]+, %r[0-9]+}}
; GEN:       {{store %r[0-9]+, %r[0-9]+}}
;
; DW-LABEL: load_store_rex:
; DW:       {{load %r[0-9]+[dq]?, %r[0-9]+[dq]?}}
; DW:       {{store %r[0-9]+[dq]?, %r[0-9]+[dq]?}}
;
; QW-LABEL: load_store_rex:
; QW:       {{load %r[0-9]+[dq]?, %r[0-9]+[dq]?}}
; QW:       {{store %r[0-9]+[dq]?, %r[0-9]+[dq]?}}
  %v = load i16, i16* %p
  store i16 %v, i16* %p
  ret i16 %v
}
