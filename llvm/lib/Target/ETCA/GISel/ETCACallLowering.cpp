//===- ETCACallLowering.cpp - ETCA Call Lowering -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ETCA call lowering for GlobalISel.
//
// Calling convention (SAF ABI):
//   - First 2 i16/i32 arguments in r0-r1, or first 2 i64 in r0/r1 pairs
//   - Actually, first 4 arguments in r0-r3 when width allows
//   - Remaining arguments on stack (aligned to register width)
//   - Return value in r0 (or r0-r1 pair for i64)
//
// Without SAF, function calls are not supported.
//===----------------------------------------------------------------------===//

#include "ETCACallLowering.h"
#include "ETCAMachineFunctionInfo.h"
#include "ETCARegisterInfo.h"
#include "ETCASubtarget.h"

#define GET_INSTRINFO_ENUM
#include "ETCAGenInstrInfo.inc"

#define GET_REGINFO_ENUM
#include "ETCAGenRegisterInfo.inc"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "etca-call-lowering"

using namespace llvm;
using namespace ETCA;

ETCACallLowering::ETCACallLowering(const TargetLowering *TLI)
    : CallLowering(TLI) {}

// Argument registers: base set (r0-r3) and REX-extended set (r0-r4, r7-r12)
// Note: r5=bp, r6=sp are reserved and not used for argument passing.
static const MCPhysReg ArgRegs16[] = {ETCA::R0, ETCA::R1, ETCA::R2, ETCA::R3};
static const MCPhysReg ArgRegs32[] = {ETCA::D0, ETCA::D1, ETCA::D2, ETCA::D3};
static const MCPhysReg ArgRegs64[] = {ETCA::Q0, ETCA::Q1, ETCA::Q2, ETCA::Q3};

// REX-extended argument registers.
static const MCPhysReg ArgRegs16_REX[] = {
    ETCA::R0, ETCA::R1, ETCA::R2,  ETCA::R3,  ETCA::R4, ETCA::R7,
    ETCA::R8, ETCA::R9, ETCA::R10, ETCA::R11, ETCA::R12};
static const MCPhysReg ArgRegs32_REX[] = {
    ETCA::D0, ETCA::D1, ETCA::D2,  ETCA::D3,  ETCA::D4, ETCA::D7,
    ETCA::D8, ETCA::D9, ETCA::D10, ETCA::D11, ETCA::D12};
static const MCPhysReg ArgRegs64_REX[] = {
    ETCA::Q0, ETCA::Q1, ETCA::Q2,  ETCA::Q3,  ETCA::Q4, ETCA::Q7,
    ETCA::Q8, ETCA::Q9, ETCA::Q10, ETCA::Q11, ETCA::Q12};

static ArrayRef<MCPhysReg> getArgRegs(unsigned WordSize, bool HasREX) {
  if (HasREX) {
    switch (WordSize) {
    case 64:
      return ArgRegs64_REX;
    case 32:
      return ArgRegs32_REX;
    default:
      return ArgRegs16_REX;
    }
  }
  switch (WordSize) {
  case 64:
    return ArgRegs64;
  case 32:
    return ArgRegs32;
  default:
    return ArgRegs16;
  }
}

/// Number of argument registers in the base (4) and REX (11) sets.
static MCRegister getRetReg(unsigned WordSize) {
  switch (WordSize) {
  case 64:
    return ETCA::Q0;
  case 32:
    return ETCA::D0;
  default:
    return ETCA::R0;
  }
}

bool ETCACallLowering::lowerReturn(MachineIRBuilder &MIRBuilder,
                                   const Value *Val, ArrayRef<Register> VRegs,
                                   FunctionLoweringInfo &FLI) const {
  if (Val && !VRegs.empty()) {
    assert(VRegs.size() == 1 && "ETCa only supports single-register returns");
    const auto &ST = MIRBuilder.getMF().getSubtarget<ETCASubtarget>();
    MachineRegisterInfo &MRI = MIRBuilder.getMF().getRegInfo();
    unsigned WordSize = ST.getWordSize();
    Register RetVal = Register(VRegs[0]);
    unsigned ValSize = MRI.getType(RetVal).getSizeInBits();
    // If the return value is narrower than the register width, sign-extend
    // it to the full register width (ETCa ABI convention).
    // If the return value is narrower than the register width, sign-extend
    // it to the full register width (ETCa ABI convention).
    // Only scalar types can be sign-extended; pointer types are copied
    // directly (they already match the pointer size).
    if (ValSize < WordSize && MRI.getType(RetVal).isScalar()) {
      RetVal = MRI.createGenericVirtualRegister(LLT::scalar(WordSize));
      MIRBuilder.buildSExt(RetVal, Register(VRegs[0]));
    }
    MIRBuilder.buildCopy(Register(getRetReg(WordSize)), RetVal);
  }
  // Use RET_Pseudo which has isReturn=1 so PEI inserts the epilogue.
  // The expander (ETCAInstrInfo::expandPostRAPseudo) converts RET_Pseudo
  // to JMPR after all MI passes (including the Control Flow Optimizer)
  // have run.  This avoids confusing non-ETCA passes with stack
  // operations in the epilogue.
  //
  // If there is a return value, also add an implicit use of the return
  // register so that the COPY to it (emitted above) is not eliminated by
  // DeadMachineInstructionElimination.  For void functions, no implicit
  // register is needed — $r0 would be undefined and cause verifier errors.
  {
    auto Ret = MIRBuilder.buildInstr(ETCA::RET_Pseudo);
    if (Val && !VRegs.empty()) {
      const auto &ST = MIRBuilder.getMF().getSubtarget<ETCASubtarget>();
      MCRegister RetReg = getRetReg(ST.getWordSize());
      Ret.addReg(RetReg, RegState::Implicit);
    }
  }
  return true;
}

bool ETCACallLowering::lowerFormalArguments(MachineIRBuilder &MIRBuilder,
                                            const Function &F,
                                            ArrayRef<ArrayRef<Register>> VRegs,
                                            FunctionLoweringInfo &FLI) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  unsigned RegWidth = ST.getRegWidth();
  unsigned RegBytes = RegWidth / 8;
  auto ArgRegs = getArgRegs(RegWidth, ST.hasREX());
  unsigned NumArgRegs = ArgRegs.size();

  // R7 is the link register (return address).  Mark it live-in on the
  // entry block so the register allocator knows it's defined there.
  // Liveness propagation through the CFG will make it live at all blocks
  // that use it (e.g., the return block which emits JMPR $r7).
  // Do NOT add R7 as live-in to non-entry blocks — LLVM's machine
  // verifier rejects allocatable live-ins on non-entry blocks.
  if (ST.hasSAF())
    MIRBuilder.getMBB().addLiveIn(ETCA::R7);

  // R6 is the stack pointer and is reserved.  Mark it as live-in on the
  // entry block so that ADJCALLSTACKDOWN instructions (used by libcalls)
  // have a valid definition of $r6 on all paths.  $r6 is defined once at
  // function entry (by the prologue) and then implicitly defined by each
  // ADJCALLSTACKDOWN (via implicit-def).
  MIRBuilder.getMBB().addLiveIn(ETCA::R6);

  if (F.arg_empty())
    return true;

  unsigned Idx = 0;
  // Track the current stack offset for incoming stack arguments.
  // The first stack argument (Arg N+1 where N = NumArgRegs) begins at
  // offset 0 from the initial SP (before the prologue).  Each subsequent
  // argument is placed at the next naturally-aligned offset.
  unsigned StackOffset = 0;
  for (auto &Arg : F.args()) {
    if (Idx >= VRegs.size())
      break;
    if (VRegs[Idx].empty()) {
      ++Idx;
      continue;
    }
    Register VReg = VRegs[Idx][0];

    if (Idx < NumArgRegs) {
      // Register argument: add live-in and copy to vreg.
      // If the vreg type is narrower than the physical register (e.g., s16
      // argument in D0, which is 32-bit), copy to a temp of the full width
      // first, then truncate.
      MIRBuilder.getMBB().addLiveIn(ArgRegs[Idx]);
      LLT VRegTy = MRI.getType(VReg);
      if (VRegTy.isScalar() && VRegTy.getSizeInBits() < RegWidth) {
        // Narrow scalar argument in a wider register: copy to a temp of
        // the full width first, then truncate.
        Register WideVReg =
            MRI.createGenericVirtualRegister(LLT::scalar(RegWidth));
        MIRBuilder.buildCopy(WideVReg, Register(ArgRegs[Idx]));
        MIRBuilder.buildTrunc(VReg, WideVReg);
      } else {
        // Same-width scalar or pointer: direct copy.
        MIRBuilder.buildCopy(VReg, Register(ArgRegs[Idx]));
      }
    } else {
      // Stack argument: create frame index at the current stack offset,
      // then advance the offset by the argument's actual size (padded
      // to register-width alignment for simplicity).
      Type *ArgTy = Arg.getType();
      unsigned ArgSize =
          ArgTy ? (MF.getDataLayout().getTypeAllocSize(ArgTy) * 8) : RegWidth;
      // Align to register width (natural alignment for the stack)
      unsigned ArgBytes = (ArgSize + 7) / 8;
      unsigned AlignedBytes = alignTo(ArgBytes, RegBytes);

      int FI = MF.getFrameInfo().CreateFixedObject(ArgBytes, StackOffset, true);
      LLT PtrTy = LLT::pointer(0, ST.getPtrSize());
      Register AddrReg = MRI.createGenericVirtualRegister(PtrTy);
      MIRBuilder.buildFrameIndex(AddrReg, FI);
      auto MMO = MF.getMachineMemOperand(
          MachinePointerInfo::getFixedStack(MF, FI), MachineMemOperand::MOLoad,
          ArgBytes, Align(RegBytes));
      MIRBuilder.buildLoad(VReg, AddrReg, *MMO);

      StackOffset += AlignedBytes;
    }

    ++Idx;
  }

  // Save vararg registers if this is a variadic function.
  if (F.isVarArg())
    saveVarArgRegisters(MIRBuilder, Idx);

  return true;
}

bool ETCACallLowering::lowerCall(MachineIRBuilder &MIRBuilder,
                                 CallLoweringInfo &Info) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  unsigned RegWidth = ST.getRegWidth();
  unsigned RegBytes = RegWidth / 8;
  auto ArgRegs = getArgRegs(RegWidth, ST.hasREX());
  unsigned NumArgRegs = ArgRegs.size();

  // --- Handle stack arguments (args beyond register count) ---
  size_t NumArgs = Info.OrigArgs.size();
  size_t NumStackArgs = (NumArgs > NumArgRegs) ? (NumArgs - NumArgRegs) : 0;
  unsigned StackArgSize = NumStackArgs * RegBytes;

  // Ensure $r6 is live-in to this block so that ADJCALLSTACKDOWN's
  // implicit use of $r6 is satisfied.  $r6 is reserved (SP) and not
  // explicitly defined in the MIR; LiveRangeCalc requires it to be
  // live-in to any block that uses it.
  MIRBuilder.getMBB().addLiveIn(ETCA::R6);

  // Emit ADJCALLSTACKDOWN marker — the actual SUBI R6, Amount is
  // emitted later by eliminateCallFramePseudoInstr in PEI.
  auto CallSeqStart = MIRBuilder.buildInstr(ETCA::ADJCALLSTACKDOWN);

  // Store each excess argument at the correct stack offset.
  for (size_t i = NumArgRegs; i < NumArgs; ++i) {
    Register ArgReg = Info.OrigArgs[i].Regs[0];
    if (!ArgReg)
      continue;

    // Determine the store opcode from the argument's bit width.
    LLT ArgTy = MRI.getType(ArgReg);
    unsigned ArgSize = ArgTy.getSizeInBits();
    unsigned StoreOpc;
    if (ArgSize >= 64)
      StoreOpc = ETCA::STORE64;
    else if (ArgSize >= 32)
      StoreOpc = ETCA::STORE32;
    else
      StoreOpc = ETCA::STORE16;

    unsigned Offset = (i - NumArgRegs) * RegBytes;

    if (Offset == 0) {
      auto Store = MIRBuilder.buildInstr(StoreOpc);
      Store.addUse(ArgReg);
      Store.addUse(Register(ETCA::R6));
    } else {
      // Create address register by copying SP (R6).  Use a GPR COPY
      // (via buildCopy) rather than a target MOVZ instruction so that
      // the generic code (regbankselect, CSE) handles it correctly.
      // Use pointer type for G_PTR_ADD compatibility.
      LLT AddrTy = LLT::pointer(0, ST.getPtrSize());
      Register CurAddr = MRI.createGenericVirtualRegister(AddrTy);
      MIRBuilder.buildCopy(CurAddr, Register(ETCA::R6));

      int64_t RemOff = static_cast<int64_t>(Offset);
      while (RemOff > 0) {
        int64_t Step = std::min<int64_t>(RemOff, 15);
        // Use G_PTR_ADD + G_CONSTANT for address arithmetic.
        // Generic opcodes work correctly in SSA form before RegBankSelect.
        // The instruction selector handles G_PTR_ADD natively.
        LLT OffTy = LLT::scalar(ST.getPtrSize());
        Register OffsetReg = MRI.createGenericVirtualRegister(OffTy);
        MIRBuilder.buildConstant(OffsetReg, Step);
        Register NextAddr = MRI.createGenericVirtualRegister(AddrTy);
        MIRBuilder.buildInstr(TargetOpcode::G_PTR_ADD)
            .addDef(NextAddr)
            .addUse(CurAddr)
            .addUse(OffsetReg);
        RemOff -= Step;
        CurAddr = NextAddr;
      }

      auto Store = MIRBuilder.buildInstr(StoreOpc);
      Store.addUse(ArgReg);
      Store.addUse(CurAddr);
    }
  }

  // --- Build the call instruction ---
  MachineInstrBuilder CallInst =
      MIRBuilder.buildInstrNoInsert(ETCA::CALL_Pseudo);

  if (Info.Callee.isReg())
    CallInst.addReg(Info.Callee.getReg());
  else if (Info.Callee.isGlobal())
    CallInst.addGlobalAddress(Info.Callee.getGlobal());
  else if (Info.Callee.isSymbol())
    CallInst.addExternalSymbol(Info.Callee.getSymbolName());

  // Add the call-preserved register mask so the register allocator
  // knows which registers are preserved by this call.
  const auto &TRI = *MF.getSubtarget().getRegisterInfo();
  CallInst.addRegMask(TRI.getCallPreservedMask(MF, Info.CallConv));

  for (unsigned i = 0, e = std::min(NumArgs, size_t(NumArgRegs)); i < e; ++i) {
    Register ArgReg = Info.OrigArgs[i].Regs[0];
    if (ArgReg) {
      // If the argument is narrower than the register width, sign-extend
      // it to the full register width before copying to the physreg.
      // Only scalar types are sign-extended; pointer types are copied directly.
      unsigned ArgSize = MRI.getType(ArgReg).getSizeInBits();
      if (ArgSize < RegWidth && MRI.getType(ArgReg).isScalar()) {
        Register WideArg =
            MRI.createGenericVirtualRegister(LLT::scalar(RegWidth));
        MIRBuilder.buildSExt(WideArg, ArgReg);
        MIRBuilder.buildCopy(Register(ArgRegs[i]), WideArg);
      } else {
        MIRBuilder.buildCopy(Register(ArgRegs[i]), ArgReg);
      }
      CallInst.addReg(ArgRegs[i], RegState::Implicit);
    }
  }

  MCRegister RetReg = getRetReg(RegWidth);
  CallInst.addDef(RetReg, RegState::ImplicitDefine);

  MIRBuilder.insertInstr(CallInst);

  // --- Emit ADJCALLSTACKUP marker — the actual ADDI R6, Amount is
  //     emitted later by eliminateCallFramePseudoInstr in PEI.
  auto CallSeqEnd = MIRBuilder.buildInstr(ETCA::ADJCALLSTACKUP);

  // Set the stack size on both markers (amt1 = size, amt2 = alignment = 0).
  CallSeqStart.addImm(StackArgSize).addImm(0);
  CallSeqEnd.addImm(StackArgSize).addImm(0);

  if (!Info.OrigRet.Regs.empty()) {
    Register RetVReg = Info.OrigRet.Regs[0];
    if (RetVReg) {
      // If the return vreg is narrower than the physical register,
      // copy to a wide temp first, then truncate.
      // Only scalar types can be truncated; pointer types are copied directly.
      LLT RetTy = MRI.getType(RetVReg);
      if (RetTy.getSizeInBits() < RegWidth && RetTy.isScalar()) {
        Register WideRet =
            MRI.createGenericVirtualRegister(LLT::scalar(RegWidth));
        MIRBuilder.buildCopy(WideRet, Register(RetReg));
        MIRBuilder.buildTrunc(RetVReg, WideRet);
      } else {
        MIRBuilder.buildCopy(RetVReg, Register(RetReg));
      }
    }
  }

  return true;
}

void ETCACallLowering::saveVarArgRegisters(MachineIRBuilder &MIRBuilder,
                                           unsigned NumNamedArgRegs) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  unsigned RegWidth = ST.getRegWidth();
  unsigned RegBytes = RegWidth / 8;
  auto ArgRegs = getArgRegs(RegWidth, ST.hasREX());
  unsigned NumRegs = ArgRegs.size();

  // Number of unallocated argument registers that could hold varargs.
  unsigned NumFreeRegs =
      (NumNamedArgRegs < NumRegs) ? (NumRegs - NumNamedArgRegs) : 0;
  unsigned VarArgsSaveSize = NumFreeRegs * RegBytes;

  // Create a stack object for the varargs save area.
  // Use a regular (non-fixed) stack object so that PEI can handle it
  // without issues.  Fixed stack objects at negative offsets can cause
  // hangs in the prologue/epilogue insertion pass.
  int FI;
  if (NumFreeRegs == 0) {
    // All varargs on the stack — use a small stack object as placeholder.
    // G_VASTART will initialize the va_list pointer to the address of
    // this object; the actual stack arguments start at the next slot.
    FI = MF.getFrameInfo().CreateStackObject(RegBytes, Align(RegBytes), false);
  } else {
    // Create a stack object to hold the saved vararg registers.
    FI = MF.getFrameInfo().CreateStackObject(VarArgsSaveSize, Align(RegBytes),
                                             false);

    const LLT sXLen = LLT::scalar(RegWidth);

    // Copy each unallocated argument register to the save area on the stack.
    for (unsigned I = NumNamedArgRegs; I < NumRegs; ++I) {
      MIRBuilder.getMBB().addLiveIn(ArgRegs[I]);

      // Copy the physreg to a virtual register of the full register width.
      Register VReg = MRI.createGenericVirtualRegister(sXLen);
      MIRBuilder.buildCopy(VReg, Register(ArgRegs[I]));

      // Compute the address of this register's slot in the save area.
      unsigned Offset = (I - NumNamedArgRegs) * RegBytes;
      LLT AddrTy = LLT::pointer(0, ST.getPtrSize());
      Register FIAddr = MRI.createGenericVirtualRegister(AddrTy);
      MIRBuilder.buildFrameIndex(FIAddr, FI);

      if (Offset > 0) {
        // Add the offset to the frame index base.
        Register OffsetReg = MRI.createGenericVirtualRegister(sXLen);
        MIRBuilder.buildConstant(OffsetReg, Offset);
        Register Addr = MRI.createGenericVirtualRegister(AddrTy);
        MIRBuilder.buildPtrAdd(Addr, FIAddr, OffsetReg);
        FIAddr = Addr;
      }

      auto MPO = MachinePointerInfo::getStack(MF, Offset);
      auto MMO = MF.getMachineMemOperand(MPO, MachineMemOperand::MOStore, sXLen,
                                         Align(RegBytes));
      MIRBuilder.buildStore(VReg, FIAddr, *MMO);
    }
  }

  // Record the frame index for G_VASTART legalization.  G_VASTART needs
  // to store the address of this stack object into the va_list pointer
  // so that va_arg can find the first vararg.
  MF.getInfo<ETCAMachineFunctionInfo>()->setVarArgsFrameIndex(FI);
}
