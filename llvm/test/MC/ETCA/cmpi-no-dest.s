# Regression test: CMPI/TESTI must have NO destination register
#
# The CMP and TEST opcodes (CCCC=0011/0111) do not write a result register
# per the spec.  Previously they used the tied-def RI format (EInstRI) which
# had (outs GPR:$dst) and $src1=$dst constraint, causing TwoAddress pass
# to insert dead COPY instructions.
#
# After the fix, CMPI/TESTI use EInstCmpRI (no-dest) with 2 operands:
#   [src1, imm] — no output register.
#
# Each CHECK verifies that the MCInst has exactly 2 operands (src, imm)
# with no tied-dst duplicating the register.
#
# RUN: llvm-mc -arch=etca -show-inst < %s | FileCheck %s

cmp %r0h, 5
# CHECK: CMPI8
# CHECK: Reg:R0
# CHECK-NOT: Reg:R0
# CHECK: Imm:5

cmp %r3, -1
# CHECK: CMPI16
# CHECK: Reg:R3
# CHECK-NOT: Reg:R3
# CHECK: Imm:-1

cmp %r5d, 31
# CHECK: CMPI32
# CHECK: Reg:D5
# CHECK-NOT: Reg:D5
# CHECK: Imm:31

cmp %r7q, 15
# CHECK: CMPI64
# CHECK: Reg:Q7
# CHECK-NOT: Reg:Q7
# CHECK: Imm:15

test %r0h, 0
# CHECK: TESTI8
# CHECK: Reg:R0
# CHECK-NOT: Reg:R0
# CHECK: Imm:0

test %r5, 31
# CHECK: TESTI16
# CHECK: Reg:R5
# CHECK-NOT: Reg:R5
# CHECK: Imm:31

test %r1d, -8
# CHECK: TESTI32
# CHECK: Reg:D1
# CHECK-NOT: Reg:D1
# CHECK: Imm:-8

test %r3q, 0
# CHECK: TESTI64
# CHECK: Reg:Q3
# CHECK-NOT: Reg:Q3
# CHECK: Imm:0
