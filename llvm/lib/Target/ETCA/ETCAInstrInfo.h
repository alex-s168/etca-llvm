//===- ETCAInstrInfo.h - ETCA Instruction Information ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_ETCAINSTRINFO_H
#define LLVM_LIB_TARGET_ETCA_ETCAINSTRINFO_H

#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "ETCAGenInstrInfo.inc"

namespace llvm {

class ETCASubtarget;
struct ETCARegisterInfo;

class ETCAInstrInfo : public ETCAGenInstrInfo {
public:
  explicit ETCAInstrInfo(ETCASubtarget &ST, const ETCARegisterInfo &RI);

  ~ETCAInstrInfo() override = default;

  const TargetRegisterInfo &getRegisterInfo() const;

  bool isMoveInstr(const TargetRegisterInfo &TRI, const MachineInstr &MI) const;

  /// Recognize MOVZ/MOVS as copy instructions for the register coalescer.
  /// Only returns a result when BOTH operands are registers (non-register
  /// operands like FrameIndex or GlobalAddress are NOT copies).
  std::optional<DestSourcePair>
  isCopyInstrImpl(const MachineInstr &MI) const override;

  Register isLoadFromStackSlot(const MachineInstr &MI,
                               int &FrameIndex) const override;

  Register isStoreToStackSlot(const MachineInstr &MI,
                              int &FrameIndex) const override;

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  void storeRegToStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register SrcReg,
      bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  void loadRegFromStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register DestReg,
      int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      unsigned SubReg = 0,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  /// Get the number of bytes pushed/popped for a given register class.
  unsigned getRegClassSize(const TargetRegisterClass &RC) const;

  /// Expand pseudo instructions into real instructions after register
  /// allocation (post-RA).  Handles SELECT_Pseudo, CALL_Pseudo, RET_Pseudo.
  bool expandPostRAPseudo(MachineInstr &MI) const override;

private:
  const ETCARegisterInfo &RegInfo;
  ETCASubtarget &ST;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_ETCAINSTRINFO_H
