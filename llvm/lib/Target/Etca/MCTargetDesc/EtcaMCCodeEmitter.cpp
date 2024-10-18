//===-- EtcaMCCodeEmitter.cpp - Convert Etca code to machine code -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EtcaMCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EtcaMCTargetDesc.h"
#include "MCTargetDesc/EtcaMCExpr.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdint>

#define DEBUG_TYPE "mccodeemitter"

STATISTIC(MCNumEmitted, "Number of MC instructions emitted");

namespace llvm {

namespace {

class EtcaMCCodeEmitter : public MCCodeEmitter {
  MCContext& Ctx;

public:
  EtcaMCCodeEmitter(const MCInstrInfo &MCII, MCContext &C):
      Ctx(C){}
  EtcaMCCodeEmitter(const EtcaMCCodeEmitter &) = delete;
  void operator=(const EtcaMCCodeEmitter &) = delete;
  ~EtcaMCCodeEmitter() override = default;

  // The functions below are called by TableGen generated functions for getting
  // the binary encoding of instructions/opereands.
  uint32_t getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;
  uint32_t getImm5OpValue(const MCInst &MI, unsigned OpNo,
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &STI) const;

  // getBinaryCodeForInstr - TableGen'erated function for getting the
  // binary encoding for an instruction.
  uint64_t getBinaryCodeForInstr(const MCInst &Inst,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &SubtargetInfo) const;

  void encodeInstruction(const MCInst &Inst, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &SubtargetInfo) const override;
};

} // end anonymous namespace

void EtcaMCCodeEmitter::encodeInstruction(
    const MCInst &Inst, SmallVectorImpl<char> &CB,
    SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &SubtargetInfo) const {
  // Get instruction encoding and emit it
  unsigned Value = getBinaryCodeForInstr(Inst, Fixups, SubtargetInfo);
  ++MCNumEmitted; // Keep track of the number of emitted insns.

  support::endian::write<uint32_t>(CB, Value, llvm::endianness::little);
}

#include "EtcaGenMCCodeEmitter.inc"

uint32_t EtcaMCCodeEmitter::getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm()) {
    uint32_t Res = static_cast<uint32_t>(MO.getImm());
    return Res;
  }

  report_fatal_error("Unhandled expression!");
  return 0;
}

uint32_t EtcaMCCodeEmitter::getImm5OpValue(const MCInst &MI, unsigned OpNo,
                        SmallVectorImpl<MCFixup> &Fixups,
                        const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  uint32_t Res = static_cast<uint32_t>(MO.getImm());
  return (Res & 0x1f);
}

} // end namespace llvm

llvm::MCCodeEmitter *
llvm::createEtcaMCCodeEmitter(const MCInstrInfo &InstrInfo,
                               MCContext &context) {
  return new EtcaMCCodeEmitter(InstrInfo, context);
}
