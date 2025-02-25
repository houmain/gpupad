cmake_minimum_required(VERSION 3.5)
project(dllreflect-test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(dllreflect-test
  test.cpp
  include/dllreflect.h
)
