//===- EtcaInstrInfo.h - Etca Instruction Information ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Etca implementation of the EtcaInstrInfo class.
//
//===----------------------------------------------------------------------===//


#ifndef LLVM_LIB_TARGET_ETCA_ETCAINSTRINFO_H
#define LLVM_LIB_TARGET_ETCA_ETCAINSTRINFO_H

#include "Etca.h"
#include "EtcaRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_INSTRINFO_HEADER
#define GET_INSTRINFO_ENUM
#include "EtcaGenInstrInfo.inc"

namespace llvm {

class EtcaTargetMachine;
class EtcaSubtarget;
class EtcaInstrInfo : public EtcaGenInstrInfo {
  const EtcaRegisterInfo RI;
  const EtcaSubtarget &STI;

public:
  EtcaInstrInfo(const EtcaSubtarget &STI);

  void adjustStackPtr(unsigned SP, int64_t Amount, MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator I) const;

  // Return the EtcaRegisterInfo, which this class owns.
  const EtcaRegisterInfo &getRegisterInfo() const { return RI; }

  Register isLoadFromStackSlot(const MachineInstr &MI,
                               int &FrameIndex) const override;

  Register isStoreToStackSlot(const MachineInstr &MI,
                              int &FrameIndex) const override;

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                   const DebugLoc &DL, MCRegister DestReg, MCRegister SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI, Register SrcReg,
                           bool isKill, int FrameIndex,
                           const TargetRegisterClass *RC,
                           const TargetRegisterInfo *TRI,
                           Register VReg) const override;

  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI, Register DestReg,
                            int FrameIdx, const TargetRegisterClass *RC,
                            const TargetRegisterInfo *TRI,
                            Register VReg) const override;

  // Get the load and store opcodes for a given register class and offset.
  void getLoadStoreOpcodes(const TargetRegisterClass *RC, unsigned &LoadOpcode,
                           unsigned &StoreOpcode, int64_t offset) const;

  // Emit code before MBBI in MI to move immediate value Value into
  // physical register Reg.
  void loadImmediate(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                     unsigned *Reg, int64_t Value) const;

  const EtcaSubtarget &getSubtarget() const { return STI; }
};
} // end namespace llvm

#endif /* LLVM_LIB_TARGET_ETCA_ETCAINSTRINFO_H */
