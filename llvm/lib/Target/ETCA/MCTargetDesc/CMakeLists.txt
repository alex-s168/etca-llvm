include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)

add_llvm_component_library(LLVMETCADesc
  ETCAMCTargetDesc.cpp
  ETCAMCAsmInfo.cpp

  LINK_COMPONENTS
  ETCAInfo
  MC
  MCDisassembler
  Support
  TargetParser

  ADD_TO_COMPONENT
  ETCA
)
