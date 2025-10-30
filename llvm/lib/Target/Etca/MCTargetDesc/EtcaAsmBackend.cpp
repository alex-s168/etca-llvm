//===-- EtcaAsmBackend.cpp - Etca Assembler Backend ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EtcaMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class EtcaObjectWriter : public MCELFObjectTargetWriter {
public:
  EtcaObjectWriter(uint8_t OSABI);

  virtual ~EtcaObjectWriter();

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override;
  bool needsRelocateWithSymbol(const MCValue &Val, const MCSymbol &Sym,
                               unsigned Type) const override;
};
} // namespace

EtcaObjectWriter::EtcaObjectWriter(uint8_t OSABI)
    : MCELFObjectTargetWriter(false, OSABI, ELF::EM_ETCA,
                              /*HasRelocationAddend=*/true) {}

EtcaObjectWriter::~EtcaObjectWriter() {}

unsigned EtcaObjectWriter::getRelocType(MCContext &Ctx, const MCValue &Target,
                                        const MCFixup &Fixup,
                                        bool IsPCRel) const {
  return ELF::R_ETCA_NONE;
}

bool EtcaObjectWriter::needsRelocateWithSymbol(const MCValue &,
                                               const MCSymbol &,
                                               unsigned Type) const {
  return false;
}

namespace llvm {
class MCObjectTargetWriter;
class EtcaMCAsmBackend : public MCAsmBackend {
  uint8_t OSABI;

public:
  EtcaMCAsmBackend(uint8_t osABI, bool isLE)
      : MCAsmBackend(llvm::endianness::little),
        OSABI(osABI) {}

  unsigned getNumFixupKinds() const override {
    return 0;
  }
  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override;
  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const override;
  bool mayNeedRelaxation(const MCInst &Inst,
                         const MCSubtargetInfo &STI) const override;
  void relaxInstruction(MCInst &Inst,
                        const MCSubtargetInfo &STI) const override;
  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;

  std::unique_ptr<MCObjectTargetWriter> createObjectTargetWriter() const override {
    return std::make_unique<EtcaObjectWriter>(OSABI);
  }
};
} // namespace llvm

const MCFixupKindInfo &
EtcaMCAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  assert(false);
}

static uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx) {
  unsigned Kind = Fixup.getKind();
  switch (Kind) {
  default:
    llvm_unreachable("Unknown fixup kind!");
  }
}

static unsigned getSize(unsigned Kind) {
  switch (Kind) {
  default:
    return 3;
  }
}

void EtcaMCAsmBackend::applyFixup(const MCAssembler &Asm,
                                    const MCFixup &Fixup, const MCValue &Target,
                                    MutableArrayRef<char> Data, uint64_t Value,
                                    bool IsResolved,
                                    const MCSubtargetInfo *STI) const {

}

bool EtcaMCAsmBackend::mayNeedRelaxation(const MCInst &Inst,
                                           const MCSubtargetInfo &STI) const {
  return false;
}

void EtcaMCAsmBackend::relaxInstruction(MCInst &Inst,
                                          const MCSubtargetInfo &STI) const {}

bool EtcaMCAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                      const MCSubtargetInfo *STI) const {
  assert(false);
}

MCAsmBackend *llvm::createEtcaMCAsmBackend(const Target &T,
                                             const MCSubtargetInfo &STI,
                                             const MCRegisterInfo &MRI,
                                             const MCTargetOptions &Options) {
  uint8_t OSABI =
      MCELFObjectTargetWriter::getOSABI(STI.getTargetTriple().getOS());
  return new llvm::EtcaMCAsmBackend(OSABI, true);
}
