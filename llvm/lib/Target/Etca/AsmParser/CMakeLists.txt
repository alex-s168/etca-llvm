add_llvm_component_library(LLVMEtcaAsmParser
  EtcaAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  Support
  EtcaDesc
  EtcaInfo

  ADD_TO_COMPONENT
  Etca
  )
