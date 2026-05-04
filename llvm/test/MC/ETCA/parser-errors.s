# RUN: not llvm-mc -arch=etca -mcpu=generic %s 2>&1 | FileCheck %s

# Test parser error messages for invalid inputs.

#=== Invalid mnemonic ===
# CHECK: unknown instruction mnemonic
  invalid_mnemonic %r0, %r1

#=== Missing operand ===
# CHECK: instruction expects 2 operands (dst, src2 or imm)
  add %r0

#=== Too many operands ===
# CHECK: instruction expects 2 operands (dst, src2 or imm)
  add %r0, %r1, %r2

#=== Register width mismatch ===
# CHECK: register width mismatch in operands
  add %r0, %r1d

#=== Pseudo-instructions in asm ===
# CHECK: pseudo-instruction 'ret' is not valid in assembly
  ret

# CHECK: pseudo-instruction 'select' is not valid in assembly
  select %r0, %r1, 1, 0

#=== Unknown label reference in ALU op (expression where imm expected) ===
# Note: labels can't be used as immediate operands in ALU operations.
# CHECK: second operand must be a register or integer immediate
  add %r0, label1

#=== Immediate for branch must be resolved ===
# Note: actually branches do accept labels. This just verifies it assembles.
# TODO: add label-relocation test once .o file emission works.
