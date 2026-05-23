// Test that clang can compile and link ETCA code using LLD (the default linker).
// RUN: %clang --target=etca-unknown-elf -mcpu=generic -nostdlib -Wl,--no-gc-sections -o %t.elf %s
// RUN: llvm-readobj -h %t.elf | FileCheck %s --check-prefix=ELF32
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DISASM
//
// Also test 32-bit and 64-bit word/ptr combinations via -m feature flags.
// RUN: %clang --target=etca-unknown-elf -mcpu=generic -m32bit -mptr32 -mdw -nostdlib -Wl,--no-gc-sections -o %t32.elf %s
// RUN: llvm-readobj -h %t32.elf | FileCheck %s --check-prefix=ELF32
//
// RUN: %clang --target=etca-unknown-elf -mcpu=generic -m64bit -mptr64 -mdw -mqw -nostdlib -Wl,--no-gc-sections -o %t64.elf %s
// RUN: llvm-readobj -h %t64.elf | FileCheck %s --check-prefix=ELF64

// ELF32: Format: elf32-{{.*}}
// ELF32: Arch: etca
// ELF32: Machine: 0xE7CA
//
// ELF64: Format: elf64-{{.*}}
// ELF64: Arch: etca
// ELF64: AddressSize: 64bit
// ELF64: Machine: 0xE7CA
//
// DISASM: <add>:
// DISASM-NEXT: push

int add(int a, int b) {
    return a + b;
}
