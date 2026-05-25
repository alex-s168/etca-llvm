# RUN: llvm-mc -arch=etca -filetype=obj %s -o - | llvm-readobj -r - \
# RUN:     | FileCheck %s
# RUN: llvm-mc -arch=etca %s -o - | FileCheck %s --check-prefix=ASM
# RUN: llvm-mc -arch=etca -mattr=+32bit,+ptr32 -filetype=obj %s -o - \
# RUN:     | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC32
# RUN: llvm-mc -arch=etca -mattr=+64bit,+ptr64 -filetype=obj %s -o - \
# RUN:     | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC64

# Test that MOVZ/MOVS with label operands produce correct R_ETCA_MOV_*
# spanning relocations, matching binutils behavior.
#
# 16-bit (default): R_ETCA_MOV_16
# 32-bit (+ptr32):  R_ETCA_MOV_32
# 64-bit (+ptr64):  R_ETCA_MOV_64

.text
.globl _start
_start:
  movz %r0, my_label
  movs %r1, my_label
  movzd %r0, my_label_32
  movsd %r1, my_label_32
  movzq %r0, my_label_64
  movsq %r1, my_label_64

my_label:
  nop

my_label_32:
  nop

my_label_64:
  nop

# RELOC32: Relocations [
# RELOC32:   Section ({{[0-9]+}}) .rela.text {
# RELOC32:     R_ETCA_MOV_32
# RELOC32:   }
# RELOC32: ]

# RELOC64: Relocations [
# RELOC64:   Section ({{[0-9]+}}) .rela.text {
# RELOC64:     R_ETCA_MOV_64
# RELOC64:   }
# RELOC64: ]

# CHECK:      Relocations [
# CHECK:        Section ({{[0-9]+}}) .rela.text {
# CHECK:      0x0 R_ETCA_MOV_16
# CHECK:      0x8 R_ETCA_MOV_16
# CHECK:      0x10 R_ETCA_MOV_32
# CHECK:      0x1E R_ETCA_MOV_32
# CHECK:      0x2C R_ETCA_MOV_64
# CHECK:      0x46 R_ETCA_MOV_64
# CHECK:        }
# CHECK:      ]

# ASM:  movz %r0, my_label
# ASM:  movs %r1, my_label
# ASM:  my_label:
# ASM:  nop
