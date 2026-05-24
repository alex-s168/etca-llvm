// Check that clang driver accepts ETCA target with various options.
//
// RUN: %clang -target etca-unknown-elf -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=generic -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=generic -m32bit -mptr32 -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=generic -m64bit -mptr64 -fsyntax-only %s 2>&1

void test(void) {
  int a, b;
  asm("add %0, %1" : "=r"(a) : "r"(b));
}