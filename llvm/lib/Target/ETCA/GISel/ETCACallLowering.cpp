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

// Argument registers: first 4 in r0-r3
static const MCPhysReg ArgRegs16[] = {ETCA::R0, ETCA::R1, ETCA::R2, ETCA::R3};
static const MCPhysReg ArgRegs32[] = {ETCA::D0, ETCA::D1, ETCA::D2, ETCA::D3};
static const MCPhysReg ArgRegs64[] = {ETCA::Q0, ETCA::Q1, ETCA::Q2, ETCA::Q3};

static const MCPhysReg *getArgRegs(unsigned WordSize) {
  switch (WordSize) {
  case 64:
    return ArgRegs64;
  case 32:
    return ArgRegs32;
  default:
    return ArgRegs16;
  }
}

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
    MIRBuilder.buildCopy(Register(getRetReg(ST.getWordSize())),
                         Register(VRegs[0]));
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
  const MCPhysReg *ArgRegs = getArgRegs(RegWidth);

  // R7 is the link register (return address).  Mark it live-in on the
  // entry block so the register allocator knows it's defined there.
  // Liveness propagation through the CFG will make it live at all blocks
  // that use it (e.g., the return block which emits JMPR $r7).
  // Do NOT add R7 as live-in to non-entry blocks — LLVM's machine
  // verifier rejects allocatable live-ins on non-entry blocks.
  if (ST.hasSAF())
    MIRBuilder.getMBB().addLiveIn(ETCA::R7);

  if (F.arg_empty())
    return true;

  unsigned Idx = 0;
  // Track the current stack offset for incoming stack arguments.
  // The first stack argument (Arg 5) begins at offset 0 from the
  // initial SP (before the prologue).  Each subsequent argument
  // is placed at the next naturally-aligned offset.
  unsigned StackOffset = 0;
  for (auto &Arg : F.args()) {
    if (Idx >= VRegs.size())
      break;
    if (VRegs[Idx].empty()) {
      ++Idx;
      continue;
    }
    Register VReg = VRegs[Idx][0];

    if (Idx < 4) {
      // Register argument: add live-in and copy to vreg
      MIRBuilder.getMBB().addLiveIn(ArgRegs[Idx]);
      MIRBuilder.buildCopy(VReg, Register(ArgRegs[Idx]));
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
  return true;
}

bool ETCACallLowering::lowerCall(MachineIRBuilder &MIRBuilder,
                                 CallLoweringInfo &Info) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const auto &ST = MF.getSubtarget<ETCASubtarget>();
  unsigned RegWidth = ST.getRegWidth();
  unsigned RegBytes = RegWidth / 8;
  const MCPhysReg *ArgRegs = getArgRegs(RegWidth);
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();

  // --- Handle stack arguments (args 5+) ---
  size_t NumArgs = Info.OrigArgs.size();
  size_t NumStackArgs = (NumArgs > 4) ? (NumArgs - 4) : 0;
  unsigned StackArgSize = NumStackArgs * RegBytes;

  if (StackArgSize > 0) {
    // Allocate stack space for excess arguments.
    int64_t Remaining = static_cast<int64_t>(StackArgSize);
    while (Remaining > 0) {
      int64_t Step = std::min<int64_t>(Remaining, 15);
      auto Sub = MIRBuilder.buildInstr(ETCA::SUBI16);
      Sub.addDef(Register(ETCA::R6));
      Sub.addUse(Register(ETCA::R6));
      Sub.addImm(Step);
      Remaining -= Step;
    }

    // Store each excess argument at the correct stack offset.
    for (size_t i = 4; i < NumArgs; ++i) {
      Register ArgReg = Info.OrigArgs[i].Regs[0];
      if (!ArgReg)
        continue;

      unsigned Offset = (i - 4) * RegBytes;

      if (Offset == 0) {
        auto Store = MIRBuilder.buildInstr(ETCA::STORE16);
        Store.addUse(ArgReg);
        Store.addUse(Register(ETCA::R6));
      } else {
        // Create address register using pointer-type LLT.
        LLT PtrTy = LLT::pointer(0, ST.getPtrSize());
        Register AddrReg = MRI.createGenericVirtualRegister(PtrTy);
        unsigned MovOpc = ST.getPtrSize() >= 64   ? ETCA::MOVZ64
                          : ST.getPtrSize() >= 32 ? ETCA::MOVZ32
                                                  : ETCA::MOVZ16;
        auto Mov = MIRBuilder.buildInstr(MovOpc);
        Mov.addDef(AddrReg);
        Mov.addUse(Register(ETCA::R6));

        int64_t RemOff = static_cast<int64_t>(Offset);
        while (RemOff > 0) {
          int64_t Step = std::min<int64_t>(RemOff, 15);
          unsigned AddiOpc = ST.getPtrSize() >= 64   ? ETCA::ADDI64
                             : ST.getPtrSize() >= 32 ? ETCA::ADDI32
                                                     : ETCA::ADDI16;
          auto Add = MIRBuilder.buildInstr(AddiOpc);
          Add.addDef(AddrReg);
          Add.addUse(AddrReg);
          Add.addImm(Step);
          RemOff -= Step;
        }

        auto Store = MIRBuilder.buildInstr(ETCA::STORE16);
        Store.addUse(ArgReg);
        Store.addUse(AddrReg);
      }
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

  for (unsigned i = 0, e = std::min(NumArgs, size_t(4)); i < e; ++i) {
    Register ArgReg = Info.OrigArgs[i].Regs[0];
    if (ArgReg) {
      MIRBuilder.buildCopy(Register(ArgRegs[i]), ArgReg);
      CallInst.addReg(ArgRegs[i], RegState::Implicit);
    }
  }

  MCRegister RetReg = getRetReg(RegWidth);
  CallInst.addDef(RetReg, RegState::ImplicitDefine);

  MIRBuilder.insertInstr(CallInst);

  // --- Deallocate stack space for excess args (after the call) ---
  if (StackArgSize > 0) {
    int64_t Remaining = static_cast<int64_t>(StackArgSize);
    while (Remaining > 0) {
      int64_t Step = std::min<int64_t>(Remaining, 15);
      auto Add = MIRBuilder.buildInstr(ETCA::ADDI16);
      Add.addDef(Register(ETCA::R6));
      Add.addUse(Register(ETCA::R6));
      Add.addImm(Step);
      Remaining -= Step;
    }
  }

  if (!Info.OrigRet.Regs.empty()) {
    Register RetVReg = Info.OrigRet.Regs[0];
    if (RetVReg)
      MIRBuilder.buildCopy(RetVReg, Register(RetReg));
  }

  return true;
}
