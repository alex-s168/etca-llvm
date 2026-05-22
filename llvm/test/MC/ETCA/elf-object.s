# RUN: llvm-mc -arch=etca -filetype=obj %s -o - | llvm-readobj -h - \
# RUN:     | FileCheck --check-prefix=HEADER %s
# RUN: llvm-mc -arch=etca -filetype=obj %s -o - | llvm-readobj -S - \
# RUN:     | FileCheck --check-prefix=SECTIONS %s
# RUN: llvm-mc -arch=etca -filetype=obj %s -o - | llvm-readobj -s - \
# RUN:     | FileCheck --check-prefix=SYMBOLS %s
# RUN: llvm-mc -arch=etca -filetype=obj %s -o - | llvm-objdump -d - \
# RUN:     | FileCheck --check-prefix=DISASM %s

# Test ELF object emission for ETCA:
#   - ELF header e_machine = EM_ETCA (0xE7Ca)
#   - Section headers (.text, .data, .symtab, .strtab)
#   - Symbol table entries for global/local labels
#   - Disassembly of .text section

# HEADER: Format: elf32-unknown
# HEADER: Arch: etca
# HEADER: AddressSize: 32bit
# HEADER: ElfHeader {
# HEADER:   Ident {
# HEADER:     Magic: (7F 45 4C 46)
# HEADER:     Class: 32-bit (0x1)
# HEADER:     DataEncoding: LittleEndian (0x1)
# HEADER:   }
# HEADER:   Type: Relocatable (0x1)
# HEADER:   Machine: 0xE7CA
# HEADER:   Flags [ (0x0)
# HEADER:   ]
# HEADER: }

# SECTIONS: Sections [
# SECTIONS:   Section {
# SECTIONS:     Index: 0
# SECTIONS:     Name:  (0)
# SECTIONS:     Type: SHT_NULL (0x0)
# SECTIONS:   }
# SECTIONS:   Section {
# SECTIONS:     Name: .strtab
# SECTIONS:     Type: SHT_STRTAB (0x3)
# SECTIONS:     AddressAlignment: 1
# SECTIONS:   }
# SECTIONS:   Section {
# SECTIONS:     Name: .text
# SECTIONS:     Type: SHT_PROGBITS (0x1)
# SECTIONS:     Flags [ (0x6)
# SECTIONS:       SHF_ALLOC (0x2)
# SECTIONS:       SHF_EXECINSTR (0x4)
# SECTIONS:     ]
# SECTIONS:     AddressAlignment: 4
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
# SECTIONS:     Name: .symtab
# SECTIONS:     Type: SHT_SYMTAB (0x2)
# SECTIONS:     AddressAlignment: 4
# SECTIONS:   }
# SECTIONS: ]

# SYMBOLS: Name: data_label
# SYMBOLS: Binding: Local (0x0)
# SYMBOLS: Section: .data
# SYMBOLS: Name: func
# SYMBOLS: Binding: Local (0x0)
# SYMBOLS: Section: .text
# SYMBOLS: Name: _start
# SYMBOLS: Binding: Global (0x1)
# SYMBOLS: Section: .text

# DISASM:       <_start>:
# DISASM-NEXT:    nop
# DISASM-NEXT:    add %r0, %r1
# DISASM-NEXT:    sub %r3, %r5
# DISASM-NEXT:    push %r0
# DISASM-NEXT:    pop %r1
# DISASM-NEXT:    nop
# DISASM:       <func>:
# DISASM-NEXT:    nop
# DISASM-NEXT:    push %r5
# DISASM-NEXT:    add %r5, 1
# DISASM-NEXT:    pop %r5
# DISASM-NEXT:    jmpr %r7

.text
.globl _start
_start:
  nop
  add %r0, %r1
  sub %r3, %r5
  push %r0
  pop %r1
  nop

.data
data_label:
  .short 0x1234
  .short 0x5678

.text
func:
  nop
  push %r5
  add %r5, 1
  pop %r5
  jmpr %r7