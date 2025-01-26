add_llvm_component_group(Etca)

set(LLVM_TARGET_DEFINITIONS Etca.td)

tablegen(LLVM EtcaGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM EtcaGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM EtcaGenCallingConv.inc -gen-callingconv)
tablegen(LLVM EtcaGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM EtcaGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM EtcaGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM EtcaGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM EtcaGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM EtcaGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(EtcaCommonTableGen)

add_llvm_target(EtcaCodeGen
  EtcaFrameLowering.cpp
  EtcaISelDAGToDAG.cpp
  EtcaISelLowering.cpp
  EtcaInstrInfo.cpp
  EtcaRegisterInfo.cpp
  EtcaSubtarget.cpp
  EtcaTargetMachine.cpp
  EtcaAsmPrinter.cpp

  LINK_COMPONENTS
  IPO
  GlobalISel
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  EtcaAsmParser
  EtcaDesc
  EtcaInfo
  MC
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  Etca
  )

add_subdirectory(AsmParser)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
