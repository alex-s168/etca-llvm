// RUN: not %clang_cc1 -triple etca-unknown-elf -emit-llvm %s -o /dev/null 2>&1 \
// RUN:     | FileCheck %s

// Test that invalid inline assembly constraints produce a diagnostic.

void test_invalid_constraint(void) {
  int a;
  // 'z' is not a valid ETCa constraint
  // CHECK: invalid output constraint '=z' in asm
  asm("add %0, %1" : "=z"(a) : "r"(a));
}