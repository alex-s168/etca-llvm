; RUN: llc -march=etca -mcpu=generic -filetype=obj < %s -o - | llvm-readobj -r - \
; RUN:     | FileCheck %s --check-prefix=RELOC16
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 -filetype=obj < %s -o - \
; RUN:     | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC32
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 -filetype=obj < %s -o - \
; RUN:     | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC64
; RUN: llc -march=etca -mcpu=generic -filetype=asm < %s \
; RUN:     | FileCheck %s --check-prefix=ASM16
; RUN: llc -march=etca -mcpu=generic -mattr=+32bit,+ptr32 -filetype=asm < %s \
; RUN:     | FileCheck %s --check-prefix=ASM32
; RUN: llc -march=etca -mcpu=generic -mattr=+64bit,+ptr64 -filetype=asm < %s \
; RUN:     | FileCheck %s --check-prefix=ASM64

; Test that G_GLOBAL_VALUE produces correct R_ETCA_MOV_* relocations:
;   16-bit ptr → R_ETCA_MOV_16 (reloc 31) — 4-instruction chain
;   32-bit ptr → R_ETCA_MOV_32 (reloc 32) — 7-instruction chain
;   64-bit ptr → R_ETCA_MOV_64 (reloc 29) — 13-instruction chain
;
; These spanning relocations tell the linker to invoke etca_build_mov_ri,
; which rewrites the MOVZI+SLO placeholder chain in-place.

@my_global = global i16 42, align 2

define i16 @load_global() {
  %v = load i16, ptr @my_global
  ret i16 %v
}

; RELOC16:      Relocations [
; RELOC16-NEXT:   Section ({{[0-9]+}}) .rela.text {
; RELOC16-NEXT:     0x0 R_ETCA_MOV_16 my_global
; RELOC16:      }
; RELOC16:    ]

; RELOC32:      Relocations [
; RELOC32-NEXT:   Section ({{[0-9]+}}) .rela.text {
; RELOC32:         R_ETCA_MOV_32 my_global
; RELOC32:      }
; RELOC32:    ]

; RELOC64:      Relocations [
; RELOC64-NEXT:   Section ({{[0-9]+}}) .rela.text {
; RELOC64:         R_ETCA_MOV_64 my_global
; RELOC64:      }
; RELOC64:    ]

; ASM16:      load_global:
; ASM16:      movz	%r0, my_global

; ASM32:      load_global:
; ASM32:      movz	%r0d, my_global

; ASM64:      load_global:
; ASM64:      movz	%r0q, my_global
