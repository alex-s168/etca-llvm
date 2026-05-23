# RUN: llvm-mc -arch=etca -filetype=obj %s -o - | llvm-readobj -r - \
# RUN:     | FileCheck %s
# RUN: llvm-mc -arch=etca -filetype=obj %s -o - | llvm-readobj -s - \
# RUN:     | FileCheck --check-prefix=SYMBOLS %s
# RUN: llvm-mc -arch=etca -filetype=obj %s -o - | llvm-readobj -S - \
# RUN:     | FileCheck --check-prefix=SECTIONS %s

# Test that ELF object files for ETCA contain correct relocation entries
# and section structures when external symbols are referenced.

# SECTIONS:   Section {
# SECTIONS:     Name: .text
# SECTIONS:     Type: SHT_PROGBITS (0x1)
# SECTIONS:     Flags [ (0x6)
# SECTIONS:       SHF_ALLOC (0x2)
# SECTIONS:       SHF_EXECINSTR (0x4)
# SECTIONS:     ]
# SECTIONS:   }
# SECTIONS:   Section {
# SECTIONS:     Name: .rela.text
# SECTIONS:     Type: SHT_RELA (0x4)
# SECTIONS:     Flags [ (0x40)
# SECTIONS:       SHF_INFO_LINK (0x40)
# SECTIONS:     ]
# SECTIONS:   }
# SECTIONS:   Section {
# SECTIONS:     Name: .data
# SECTIONS:     Type: SHT_PROGBITS (0x1)
# SECTIONS:     Flags [ (0x3)
# SECTIONS:       SHF_ALLOC (0x2)
# SECTIONS:       SHF_WRITE (0x1)
# SECTIONS:     ]
# SECTIONS:   }
# SECTIONS:   Section {
# SECTIONS:     Name: .rela.data
# SECTIONS:     Type: SHT_RELA (0x4)
# SECTIONS:   }

# CHECK: Relocations [
# CHECK:   Section ({{[0-9]+}}) .rela.text {
# CHECK:     0x0 R_ETCA_BASE_JMP ext_func 0x0
# CHECK:     0x2 R_ETCA_SAF_CALL ext_func 0x0
# CHECK:   }
# CHECK:   Section ({{[0-9]+}}) .rela.data {
# CHECK:     0x0 R_ETCA_16 ext_data 0x0
# CHECK:   }
# CHECK: ]

# SYMBOLS:   Symbol {
# SYMBOLS:     Name: _start
# SYMBOLS:     Binding: Global (0x1)
# SYMBOLS:     Section: .text
# SYMBOLS:   }
# SYMBOLS:   Symbol {
# SYMBOLS:     Name: ext_func
# SYMBOLS:     Binding: Global (0x1)
# SYMBOLS:     Section: Undefined (0x0)
# SYMBOLS:   }
# SYMBOLS:   Symbol {
# SYMBOLS:     Name: ext_data
# SYMBOLS:     Binding: Global (0x1)
# SYMBOLS:     Section: Undefined (0x0)
# SYMBOLS:   }

.text
.globl _start
_start:
  br ext_func
  call ext_func
  nop

.data
  .short ext_data