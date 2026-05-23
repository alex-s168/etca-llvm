# RUN: llvm-mc -arch=etca --disassemble < %s 2>&1 | FileCheck %s

# Test that invalid/malformed encodings are properly rejected by the
# disassembler rather than being decoded as something else.
#
# RI format: (rA << 13) | (imm << 8) | (0x01 << 6) | (SS << 4) | CCCC
#
# CCCC=13 (0xD) in RI format is PUSHI only when rA=6 (sp).  Other rA values
# (0-5, 7) are invalid and must be rejected, not mis-decoded as SLO16.
# SLO16 uses CCCC=12 (0xC), not 13.

# All warnings come first in the output (stderr), then valid instructions (stdout).

# ---- Invalid: CCCC=13, RegA != 6 (should be rejected, not decoded as SLO16) ----

# CCCC=13, rA=0, imm=0, SS=01
0x5d 0x00
# CHECK: warning: invalid instruction encoding

# CCCC=13, rA=1, imm=7, SS=01
0x5d 0x23
# CHECK: warning: invalid instruction encoding

# CCCC=13, rA=7, imm=31, SS=01
0x5d 0xff
# CHECK: warning: invalid instruction encoding

# CCCC=13, rA=4, imm=5, SS=01
0x5d 0x85
# CHECK: warning: invalid instruction encoding

# CCCC=13, rA=5, imm=0, SS=01
0x5d 0xa0
# CHECK: warning: invalid instruction encoding

# CCCC=13, rA=3, imm=3, SS=01
0x5d 0x63
# CHECK: warning: invalid instruction encoding

# CCCC=13, rA=0, imm=3, SS=00 (byte)
0x4d 0x03
# CHECK: warning: invalid instruction encoding

# CCCC=13, rA=7, imm=3, SS=00 (byte)
0x4d 0xe3
# CHECK: warning: invalid instruction encoding

# ---- Valid: CCCC=13 with RegA=6 (sp) => PUSHI ----

# PUSHI with rA=6 (sp), imm=31, SS=01
0x5d 0xdf
# CHECK: push 31

# PUSHI8 with rA=6 (sp), imm=31, SS=00 (byte)
0x4d 0xdf
# CHECK: push 31
