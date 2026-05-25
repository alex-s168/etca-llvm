# RUN: llvm-mc -arch=etca -filetype=obj %s -o - | llvm-readobj -r - \
# RUN:     | FileCheck %s

# Test that the existing ELF relocation infrastructure works correctly.
# The MOV_* relocations (R_ETCA_MOV_16, R_ETCA_MOV_32, etc.) are emitted
# by the encoder's emitJTChain for JT_Pseudo instructions, not from
# assembly source.  This test verifies the standard data fixup path.

.text
.globl _start
_start:
  # Jump table data: function pointer table
.LJTI0_0:
  .short bar
  .short baz

# CHECK:      Relocations [
# CHECK:        Section ({{[0-9]+}}) .rela.text {
# CHECK-NEXT:    0x0 R_ETCA_16 bar 0x0
# CHECK-NEXT:    0x2 R_ETCA_16 baz 0x0
# CHECK:        }
# CHECK:      ]
