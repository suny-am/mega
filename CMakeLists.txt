cmake_minimum_required(VERSION 3.0...3.25)
project(
  Mega
  VERSION 0.1.0
  LANGUAGES CXX C)

include(utils.cmake)

option(DEV_MODE "Set up development helper settings" ON)

find_package(Git QUIET)
if(GIT_FIND AND EXISTS "$PROJECT_SOURCE_DIR/.git")
  option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
      message(STATUS "Submodule update")
      execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive .
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      RESULT_VARIABLE GIT_SUBMOD_RESULT)
      if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
      endif()
    endif()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/glfw/CMakeLists.txt")
  message(FATAL_ERROR "glfw submodule was not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again")
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/glm/CMakeLists.txt")
  message(FATAL_ERROR "glm submodule was not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again")
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/glfw3webgpu/CMakeLists.txt")
  message(FATAL_ERROR "glfw3webgpu submodule was not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again")
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/nativefiledialog-extended/CMakeLists.txt")
  message(FATAL_ERROR "nativefiledialog-extended submodule was not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again")
endif()

if(NOT EMSCRIPTEN)
  add_subdirectory(glfw)
else()
  add_library(glfw INTERFACE)
  target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(glfw3webgpu)
add_subdirectory(webgpu)
add_subdirectory(nativefiledialog-extended)

if(NOT EXISTS imgui/CMakeLists.txt)
  file(READ CMakeLists_imgui.txt CMAKE_IMGUI)
  file(WRITE imgui/CMakeLists.txt ${CMAKE_IMGUI})
endif()

add_subdirectory(imgui)

add_executable(App  
  implementations.cpp
  main.cpp
  application.h 
  application.cpp
  controls.h
  controls.cpp 
  resource-manager.h 
  resource-manager.cpp
)

if(DEV_MODE)
  target_compile_definitions(
    App PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
else()
  target_compile_definitions(App PRIVATE RESOURCE_DIR="./resources")
endif()

target_compile_definitions(App PRIVATE GLM_FORCE_DEPTH_ZERO_TONE=1
                                       GLM_FORCE_LEFT_HANDED=1)

target_include_directories(App PRIVATE .)

target_link_libraries(App PRIVATE webgpu glfw glfw3webgpu imgui nfd)

target_treat_all_warnings_as_errors(App)
target_copy_webgpu_binaries(App)

set_target_properties(App PROPERTIES CXX_STANDARD 17)

if(XCODE)
  set_target_properties(
    App PROPERTIES XCODE_GENERATE_SCHEME ON
                   XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()

if(EMSCRIPTEN)
  set_target_properties(App PROPERTIES SUFFIX ".html")
  target_link_options((App PRICVATE -sASYNCIFY))
endif()

if(MSVC)

  # Disable warnings produced by GLM
  #
  # C4201: nonstandard extension used: nameless struct/union
  target_compile_options(App PUBLIC /wd4201)
  # C4305: truncation from 'int' to 'bool' in 'if' condition
  target_compile_options(App PUBLIC /wd4305)

  # Disable warnings produced by stb_image:
  #
  # C4244: conversion from 'int' to 'short', possible loss of data
  target_compile_options(App PUBLIC /wd4244)
endif(MSVC)
