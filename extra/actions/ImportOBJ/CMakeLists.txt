
add_library(ImportOBJ SHARED
  module.cpp
  ${CMAKE_SOURCE_DIR}/libs/dllreflect/include/dllreflect.h
)
include_directories(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/libs/dllreflect/include)
