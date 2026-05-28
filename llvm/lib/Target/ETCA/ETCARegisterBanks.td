//=- ETCARegisterBanks.td - Describe the ETCA Register Banks -*- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Register banks for GlobalISel on ETCA.
//
// ETCA has a single GPR bank.  All register classes (GPR, GPR32, GPR64)
// map to this one bank.  The bank covers all physical registers.
//
//===----------------------------------------------------------------------===//

/// General Purpose Registers bank.
/// Covers GPR8 (8-bit), GPR (16-bit), GPR32 (32-bit), and GPR64 (64-bit) register classes.
def GPRBank : RegisterBank<"GPRB", [GPR8, GPR8_REX, GPR, GPR32, GPR64]>;
