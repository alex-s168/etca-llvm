// Test that the clang driver can compile ETCa inline assembly for all width
// combinations (via -m feature flags).
//
// RUN: %clang -target etca-unknown-elf -mcpu=generic -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=generic -m32bit -mptr32 -mdw -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=generic -m64bit -mptr64 -mdw -mqw -fsyntax-only %s 2>&1
// RUN: %clang -target etca-unknown-elf -mcpu=generic -m64bit -mptr32 -mdw -mqw -fsyntax-only %s 2>&1

void test(void) {
  int a, b;
  asm("add %0, %1" : "=r"(a) : "r"(b));
}