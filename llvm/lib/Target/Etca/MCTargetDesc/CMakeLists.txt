add_llvm_component_library(LLVMEtcaDesc
  EtcaAsmBackend.cpp
  EtcaInstPrinter.cpp
  EtcaMCAsmInfo.cpp
  EtcaMCCodeEmitter.cpp
  EtcaMCExpr.cpp
  EtcaMCTargetDesc.cpp

  LINK_COMPONENTS
  EtcaInfo
  MC
  MCDisassembler
  Support
  TargetParser

  ADD_TO_COMPONENT
  Etca
)
