cmake_minimum_required(VERSION 3.14)
project(cppurcu LANGUAGES CXX)

add_library(cppurcu INTERFACE)
add_library(cppurcu::cppurcu ALIAS cppurcu)

target_include_directories(cppurcu INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(cppurcu INTERFACE cxx_std_17)
