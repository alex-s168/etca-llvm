; RUN: llc -march=etca -mcpu=generic -filetype=obj < %s -o %t.o
; RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
; RUN: ld.lld -m elf32etca %t.o -o %t.exe
; RUN: llvm-objdump -d %t.exe | FileCheck %s --check-prefix=LINKED

; Test that switch statements with many cases produce a jump table:
;   1. Object file has a R_ETCA_MOV_16 spanning relocation for the
;      jump table address, and R_ETCA_16 relocations for each entry.
;   2. Linked binary has the MOVZI+SLO chain resolved to the actual
;      .rodata address, and the jump table entries point to the
;      correct case blocks.

; RELOC:      Relocations [
; RELOC:        Section ({{[0-9]+}}) .rela.text {
; RELOC:          R_ETCA_MOV_16
; RELOC:        }
; RELOC:        Section ({{[0-9]+}}) .rela.rodata {
; RELOC:          R_ETCA_16
; RELOC:          R_ETCA_16
; RELOC:          R_ETCA_16
; RELOC:          R_ETCA_16
; RELOC:          R_ETCA_16
; RELOC:          R_ETCA_16
; RELOC:          R_ETCA_16
; RELOC:          R_ETCA_16
; RELOC:        }
; RELOC:      ]

; LINKED:      movz %r{{[0-9]+}}, 0
; LINKED-NEXT: slo %r{{[0-9]+}},
; LINKED-NEXT: slo %r{{[0-9]+}},
; LINKED:      jmpr %r{{[0-9]+}}

target triple = "etca-unknown-elf"

define i16 @jt_switch(i16 %n) {
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
  ]

c0:   ret i16 100
c1:   ret i16 101
c2:   ret i16 102
c3:   ret i16 103
c4:   ret i16 104
c5:   ret i16 105
c6:   ret i16 106
c7:   ret i16 107
default:
  ret i16 255
}
