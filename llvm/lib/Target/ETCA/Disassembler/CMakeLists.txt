include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)

add_llvm_component_library(LLVMETCADisassembler
  ETCADisassembler.cpp

  LINK_COMPONENTS
  MC
  MCDisassembler
  ETCADesc
  ETCAInfo
  Support

  ADD_TO_COMPONENT
  ETCA
  )
