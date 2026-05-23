//===- ETCACallingConv.td - ETCA Calling Conventions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//  Describe the ETCA calling convention
//
//  ETCA SAF ABI:
//    - r0-r1: argument/return value registers (caller-saved)
//    - r2:    argument register (caller-saved)
//    - r3-r4: callee-saved registers
//    - r5:    base pointer (bp) — callee-saved
//    - r6:    stack pointer (sp) — callee-saved
//    - r7:    link register (ln) — caller-saved
//
//  REX extended ABI:
//    - r8-r12 (t0-t4): caller-saved temp registers (additional args)
//    - r13-r15 (s2-s4): callee-saved registers
//
//  Arguments: first 4 in r0-r3 (REX extends to r0-r7), rest on stack
//  Return:    r0 (16/32-bit) or r0-r1 pair (64-bit)
//
//  Stack grows down.  Stack is aligned to register width bytes.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// CCIfSubtarget — match if the current subtarget has a feature F.
// This follows the same pattern as AArch64, PowerPC, Mips, etc.
//===----------------------------------------------------------------------===//

class CCIfSubtarget<string F, CCAction A>
    : CCIf<!strconcat("State.getMachineFunction()"
                      ".getSubtarget<ETCASubtarget>().",
                     F),
          A>;

//===----------------------------------------------------------------------===//
// Calling convention for formal arguments and call operands
//===----------------------------------------------------------------------===//



def CC_ETCA : CallingConv<[
  // Promote i8/i16 to word size based on subtarget:
  //   64-bit word (HasQW): promote to i64
  //   32-bit word (HasDW): promote to i32
  //   16-bit word (neither): promote i8 to i16
  CCIfType<[i8, i16], CCIfSubtarget<"hasQW()", CCPromoteToType<i64>>>,
  CCIfType<[i8, i16], CCIfSubtarget<"hasDW()", CCPromoteToType<i32>>>,
  CCIfType<[i8], CCPromoteToType<i16>>,

  // Arguments in registers: base=r0-r3, REX extends to r0-r12
  // (R4, R7 used as additional arg regs; R5-R6 are bp/sp)
  CCIfType<[i16], CCIfSubtarget<"hasREX()",
      CCAssignToReg<[R0, R1, R2, R3, R4, R7, R8, R9, R10, R11, R12]>>>,
  CCIfType<[i16], CCAssignToReg<[R0, R1, R2, R3]>>,
  CCIfType<[i32], CCIfSubtarget<"hasREX()",
      CCAssignToReg<[D0, D1, D2, D3, D4, D7, D8, D9, D10, D11, D12]>>>,
  CCIfType<[i32], CCAssignToReg<[D0, D1, D2, D3]>>,
  CCIfType<[i64], CCIfSubtarget<"hasREX()",
      CCAssignToReg<[Q0, Q1, Q2, Q3, Q4, Q7, Q8, Q9, Q10, Q11, Q12]>>>,
  CCIfType<[i64], CCAssignToReg<[Q0, Q1, Q2, Q3]>>,

  // Remaining arguments on stack, 2-byte aligned for i16, 4 for i32, 8 for i64
  CCIfType<[i16], CCAssignToStack<2, 2>>,
  CCIfType<[i32], CCAssignToStack<4, 4>>,
  CCIfType<[i64], CCAssignToStack<8, 8>>
]>;

//===----------------------------------------------------------------------===//
// Calling convention for return values
//===----------------------------------------------------------------------===//

def RetCC_ETCA : CallingConv<[
  // Promote i8/i16 returns to word size (same logic as CC_ETCA)
  CCIfType<[i8, i16], CCIfSubtarget<"hasQW()", CCPromoteToType<i64>>>,
  CCIfType<[i8, i16], CCIfSubtarget<"hasDW()", CCPromoteToType<i32>>>,
  CCIfType<[i8], CCPromoteToType<i16>>,

  // Return values in r0 or r0-r1 pair
  CCIfType<[i16], CCAssignToReg<[R0]>>,
  CCIfType<[i32], CCAssignToReg<[D0]>>,
  // i64 fits in a single Q0 register on 64-bit machines.
  // [Q0, Q1] would allocate two 64-bit registers for one 64-bit value.
  CCIfType<[i64], CCAssignToReg<[Q0]>>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved registers (CSR) masks
//
// SAF ABI callee-saved: R3, R4, R5 (bp), R6 (sp)
//   - 16-bit machines: R3, R4 (GPR class)
//   - 32-bit machines: D3, D4 (GPR32 class)
//   - 64-bit machines: Q3, Q4 (GPR64 class)
//
// R5 (bp) and R6 (sp) are always reserved and never allocated, but LLVM's
// CSR mask still includes them for proper save/restore ordering.
//
// With REX extension: R13(s2), R14(s3), R15(s4) are also callee-saved.
//===----------------------------------------------------------------------===//

def CSR_ETCA_RegMask : CalleeSavedRegs<(add R3, R4, R5, R6)>;
def CSR_ETCA_GPR32_RegMask : CalleeSavedRegs<(add D3, D4, D5, D6)>;
def CSR_ETCA_GPR64_RegMask : CalleeSavedRegs<(add Q3, Q4, Q5, Q6)>;

def CSR_ETCA_REX_RegMask : CalleeSavedRegs<(add R3, R4, R5, R6, R13, R14, R15)>;
def CSR_ETCA_REX_GPR32_RegMask : CalleeSavedRegs<(add D3, D4, D5, D6, D13, D14, D15)>;
def CSR_ETCA_REX_GPR64_RegMask : CalleeSavedRegs<(add Q3, Q4, Q5, Q6, Q13, Q14, Q15)>;

//===----------------------------------------------------------------------===//
// CSR masks for special calling conventions
//
// PreserveMost  — callee preserves all registers except R0 (return/arg reg).
//                 Used for CallingConv::PreserveMost and CallingConv::Cold
//                 (cold functions are rarely called, so minimize caller save).
// PreserveAll   — callee preserves ALL registers (including R0).
// NoRegs        — callee preserves nothing (GHC convention).
//
// Each convention has 6 variants: 16/32/64-bit × base/REX.
// See getCallPreservedMask in ETCARegisterInfo.cpp for index mapping.
//===----------------------------------------------------------------------===//

// --- PreserveMost ---
def CSR_ETCA_PreserveMost_RegMask : CalleeSavedRegs<
    (add R1, R2, R3, R4, R5, R6, R7)>;
def CSR_ETCA_PreserveMost_GPR32_RegMask : CalleeSavedRegs<
    (add D1, D2, D3, D4, D5, D6, D7)>;
def CSR_ETCA_PreserveMost_GPR64_RegMask : CalleeSavedRegs<
    (add Q1, Q2, Q3, Q4, Q5, Q6, Q7)>;

def CSR_ETCA_PreserveMost_REX_RegMask : CalleeSavedRegs<
    (add R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15)>;
def CSR_ETCA_PreserveMost_REX_GPR32_RegMask : CalleeSavedRegs<
    (add D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15)>;
def CSR_ETCA_PreserveMost_REX_GPR64_RegMask : CalleeSavedRegs<
    (add Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, Q10, Q11, Q12, Q13, Q14, Q15)>;

// --- PreserveAll ---
def CSR_ETCA_PreserveAll_RegMask : CalleeSavedRegs<
    (add R0, R1, R2, R3, R4, R5, R6, R7)>;
def CSR_ETCA_PreserveAll_GPR32_RegMask : CalleeSavedRegs<
    (add D0, D1, D2, D3, D4, D5, D6, D7)>;
def CSR_ETCA_PreserveAll_GPR64_RegMask : CalleeSavedRegs<
    (add Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7)>;

def CSR_ETCA_PreserveAll_REX_RegMask : CalleeSavedRegs<
    (add R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15)>;
def CSR_ETCA_PreserveAll_REX_GPR32_RegMask : CalleeSavedRegs<
    (add D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15)>;
def CSR_ETCA_PreserveAll_REX_GPR64_RegMask : CalleeSavedRegs<
    (add Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, Q10, Q11, Q12, Q13, Q14, Q15)>;

// --- NoRegs (GHC convention) ---
def CSR_NoRegs_RegMask : CalleeSavedRegs<(add)>;
