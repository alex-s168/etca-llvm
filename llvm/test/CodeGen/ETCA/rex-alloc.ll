; RUN: llc -mtriple=etca-unknown-elf -mcpu=generic -mattr=+rex -verify-machineinstrs < %s 2>&1 | FileCheck %s

; Test that the REX register set (r8-r15) is available for register allocation.
; The +rex feature enables the extended register classes.

; 16-bit test - verify that we can compile with REX registers
define i16 @add16_rex(i16 %a, i16 %b) {
; CHECK-LABEL: add16_rex:
; CHECK: add %r{{[0-9]+}}, %r{{[0-9]+}}
  %r = add i16 %a, %b
  ret i16 %r
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
