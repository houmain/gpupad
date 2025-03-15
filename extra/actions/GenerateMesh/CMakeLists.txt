cmake_minimum_required(VERSION 3.14)
project(GenerateMesh LANGUAGES C CXX)

add_library(${PROJECT_NAME} SHARED
  module.cpp
  ${CMAKE_SOURCE_DIR}/libs/dllreflect/include/dllreflect.h
)
include_directories(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/libs/dllreflect/include)
