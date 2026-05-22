// Test that the clang driver can compile ETCa inline assembly for all 5 CPU models.
//
// RUN: %clang -target etca-unknown-elf -mcpu=generic -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=etca32 -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=etca32p64 -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=etca64p32 -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=etca64 -fsyntax-only %s 2>&1

void test(void) {
  int a, b;
  asm("add %0, %1" : "=r"(a) : "r"(b));
}