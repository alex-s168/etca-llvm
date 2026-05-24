add_llvm_component_group(ETCA)

set(LLVM_TARGET_DEFINITIONS ETCA.td)

tablegen(LLVM ETCAGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM ETCAGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM ETCAGenCallingConv.inc -gen-callingconv)
# No -gen-dag-isel: ETCA uses GlobalISel exclusively.
# Disassembler tables are auto-generated via -gen-disassembler.
# The manual decoder in ETCADisassembler.cpp wraps the generated table
# to add REX prefix handling (extended r8-r15/d8-d15/q8-q15 registers).
tablegen(LLVM ETCAGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM ETCAGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM ETCAGenGlobalISel.inc -gen-global-isel -gisel-extended-llt)
tablegen(LLVM ETCAGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM ETCAGenRegisterBank.inc -gen-register-bank)
tablegen(LLVM ETCAGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM ETCAGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(ETCACommonTableGen)

# MC-level support libraries are in subdirectories (standard LLVM layout).
add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_subdirectory(Disassembler)

add_llvm_target(ETCACodeGen
  PARTIAL_SOURCES_INTENDED
  ETCATargetMachine.cpp
  ETCASubtarget.cpp
  ETCAInstrInfo.cpp
  ETCARegisterInfo.cpp
  ETCAISelLowering.cpp
  ETCAAsmPrinter.cpp
  ETCAFrameLowering.cpp
  ETCASelectExpand.cpp
  ETCAEliminateIdentityMoves.cpp
  GISel/ETCACallLowering.cpp
  GISel/ETCAInstructionSelector.cpp
  GISel/ETCALegalizerInfo.cpp
  GISel/ETCARegisterBankInfo.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  GlobalISel
  MC
  MCDisassembler
  MCParser
  Support
  Target
  TargetParser
  TransformUtils
  ETCADesc
  ETCAInfo

  ADD_TO_COMPONENT
  ETCA
)
