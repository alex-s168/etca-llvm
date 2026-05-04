include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)

add_llvm_component_library(LLVMETCAAsmParser
  ETCAAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  ETCADesc
  ETCAInfo
  Support

  ADD_TO_COMPONENT
  ETCA
  )
