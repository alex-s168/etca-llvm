// Test that clang's ETCA driver passes correct linker emulation flags.
//
// Default (no -fuse-ld=) should use LLD with correct emulation:
// RUN: %clang --target=etca-unknown-elf -mcpu=generic -nostdlib %s -### 2>&1 | FileCheck %s --check-prefix=LLD-GENERIC
// RUN: %clang --target=etca-unknown-elf -mcpu=etca32 -nostdlib %s -### 2>&1 | FileCheck %s --check-prefix=LLD-32
// RUN: %clang --target=etca-unknown-elf -mcpu=etca64 -nostdlib %s -### 2>&1 | FileCheck %s --check-prefix=LLD-64
// RUN: %clang --target=etca-unknown-elf -mcpu=etca32p64 -nostdlib %s -### 2>&1 | FileCheck %s --check-prefix=LLD-64
//
// Explicit -fuse-ld=lld should also work (same as default):
// RUN: %clang --target=etca-unknown-elf -mcpu=generic -fuse-ld=lld -nostdlib %s -### 2>&1 | FileCheck %s --check-prefix=LLD-GENERIC
// RUN: %clang --target=etca-unknown-elf -mcpu=etca64 -fuse-ld=lld -nostdlib %s -### 2>&1 | FileCheck %s --check-prefix=LLD-64

// LLD-GENERIC: ld.lld"
// LLD-GENERIC: "-melf32etca"
//
// LLD-32: ld.lld"
// LLD-32: "-melf32etca"
//
// LLD-64: ld.lld"
// LLD-64: "-melf64etca"

int main(void) { return 0; }
