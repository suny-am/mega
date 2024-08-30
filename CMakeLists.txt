cmake_minimum_required(VERSION 3.0...3.25)
project(
  LearnWebGPU
  VERSION 0.1.0
  LANGUAGES CXX C)

option(DEV_MODE "Set up development helper settings" ON)

if(NOT EMSCRIPTEN)
  add_subdirectory(glfw)
else()
  add_library(glfw INTERFACE)
  target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(glfw3webgpu)

add_subdirectory(webgpu)

add_executable(App main.cpp)

target_include_directories(App PRIVATE .)

if(DEV_MODE)
  target_compile_definitions(
    App PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
else()
  target_compile_definitions(App PRIVATE RESOURCE_DIR="./resources")
endif()

target_compile_definitions(
  App PRIVATE GLM_FORCE_DEPTH_ZERO_TONE=1 GLM_FORCE_LEFT_HANDED=1
)

target_link_libraries(App PRIVATE webgpu glfw glfw3webgpu)

target_copy_webgpu_binaries(App)

set_target_properties(
  App
  PROPERTIES CXX_STANDARD 17
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF
             COMPILE_WARNING_AS_ERROR ON)

if(MSVC)
  target_compile_options(App PRIVATE /W4)
  # dsiable warning C4244: conversion from 'int' to 'short', possible loss of data
  target_compile_options(App PUBLIC /wd4244)
else()
  target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()

if(XCODE)
  set_target_properties(
    App PROPERTIES XCODE_GENERATE_SCHEME ON
                   XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()

if(EMSCRIPTEN)
  set_target_properties(App PROPERTIES SUFFIX ".html")
  target_link_options((App PRICVATE -sASYNCIFY))
endif()