add_llvm_component_library(LLVMEtcaInfo
  EtcaTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  Etca
  )
