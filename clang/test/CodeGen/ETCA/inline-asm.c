// RUN: %clang_cc1 -triple etca-unknown-elf -emit-llvm %s -o - | FileCheck %s

// Test clang inline assembly constraints for ETCa:
//   - 'r' : general register
//   - 'i' : immediate constant
//   - 'm' : memory operand
//   - '0' : same-register I/O (tied operand)

// === 'r' constraint: general register ===
void test_r_constraint(void) {
  int a, b;
  // CHECK: call i16 asm "add $0, $1", "=r,r"
  asm("add %0, %1" : "=r"(a) : "r"(b));
}

// === 'i' constraint: immediate constant ===
void test_i_constraint(void) {
  int a;
  // CHECK: call i16 asm "movz $0, $1", "=r,i"
  asm("movz %0, %1" : "=r"(a) : "i"(42));
}

// === 'm' constraint: memory operand ===
void test_m_constraint(void) {
  int a;
  // CHECK: call i16 asm "load $0, $1", "=r,*m"
  asm("load %0, %1" : "=r"(a) : "m"(a));
}

// === '0' constraint: same-register I/O (tied operand) ===
void test_0_constraint(void) {
  int a;
  // CHECK: call i16 asm "add $0, $1", "=r,0"
  asm("add %0, %1" : "=r"(a) : "0"(a));
}

// === Volatile asm (side effects) ===
void test_volatile_asm(void) {
  // CHECK: call void asm sideeffect "nop", ""()
  asm volatile("nop");
}

// === Multiple register operands ===
void test_multi_operand(void) {
  int a, b, c;
  // CHECK: call i16 asm "add $0, $1, $2", "=r,r,r"
  asm("add %0, %1, %2" : "=r"(a) : "r"(b), "r"(c));
}

// === Input-only asm ===
void test_input_only(void) {
  int a;
  // CHECK: call void asm sideeffect "use $0", "r"
  asm volatile("use %0" : : "r"(a));
}

// === Clobber list ===
void test_clobber(void) {
  int a;
  // CHECK: call i16 asm "add $0, $1", "=r,r,~{r0},~{r1}"
  asm("add %0, %1" : "=r"(a) : "r"(a) : "r0", "r1");
}

// === Read-write ('+' modifier, tied operand) ===
void test_readwrite(void) {
  int a;
  // CHECK: call i16 asm "add $0, $1", "=r,r,0"
  asm("add %0, %1" : "+r"(a) : "r"(a));
}

// === Early clobber ===
void test_earlyclobber(void) {
  int a, b;
  // CHECK: call i16 asm "add $0, $1, $2", "=&r,r,r"
  asm("add %0, %1, %2" : "=&r"(a) : "r"(b), "r"(b));
}

// === Unused output with input constraint ===
void test_unused_output(void) {
  int a;
  // CHECK: call void asm sideeffect "nop", "r"
  asm volatile("nop" : : "r"(a));
}