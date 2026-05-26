//=- ETCAMachineFunctionInfo.h - ETCA machine function info ----*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares ETCA-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ETCA_ETCAMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_ETCA_ETCAMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class ETCASubtarget;

/// ETCAMachineFunctionInfo - Contains ETCA-specific information for each
/// MachineFunction.  Currently used to track the varargs save area frame
/// index for G_VASTART lowering.
class ETCAMachineFunctionInfo : public MachineFunctionInfo {
  /// FrameIndex for start of varargs save area.
  int VarArgsFrameIndex = 0;

public:
  ETCAMachineFunctionInfo(const Function &F, const ETCASubtarget *STI) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override {
    return DestMF.cloneInfo<ETCAMachineFunctionInfo>(*this);
  }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ETCA_ETCAMACHINEFUNCTIONINFO_H
