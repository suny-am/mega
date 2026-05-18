set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_minimum_required(VERSION 3.0...3.25)
project(
	LearnWebGPU
	VERSION 0.1.0
	LANGUAGES CXX C
)

if (NOT EMSCRIPTEN)
  add_subdirectory(glfw)
else()
  add_library(glfw INTERFACE)
  target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)

add_executable(App main.cpp
  implementations.cpp
  Application.h
  Application.cpp
  ResourceManager.h
  ResourceManager.cpp
  webgpu-utils.h
  webgpu-utils.cpp)

option(DEV_MODE "Set up development helper settings" ON)

target_include_directories(App PRIVATE .)

target_compile_definitions(App PRIVATE
  GLM_FORCE_DEPTH_ZERO_TO_ONE
  GLM_FORCE_LEFT_HANDED
)

if(DEV_MODE)
    # In dev mode, we load resources from the source tree, so that when we
    # dynamically edit resources (like shaders), these are correctly
    # versionned.
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
    )
else()
    # In release mode, we just load resources relatively to wherever the
    # executable is launched from, so that the binary is portable
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="./resources"
    )
endif()
# Add the 'webgpu' target as a dependency of our App
target_link_libraries(App PRIVATE webgpu glfw glfw3webgpu)

target_copy_webgpu_binaries(App)

# The application's binary must find wgpu.dll or libwgpu.so at runtime,
# so we automatically copy it (it's called WGPU_RUNTIME_LIB in general)
# next to the binary.
target_copy_webgpu_binaries(App)

set_target_properties(App PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
	target_compile_options(App PRIVATE /W4)
  target_compile_options(App PUBLIC /wd4244)
else()
	target_compile_options(App PRIVATE -Wall -Wextra -pedantic -ferror-limit=0)
endif()

if (XCODE)
	set_target_properties(App PROPERTIES
		XCODE_GENERATE_SCHEME ON
		XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
	)
endif()

# Options that are specific to Emscripten
if (EMSCRIPTEN)
	# Generate a full web page rather than a simple WebAssembly module
	set_target_properties(App PROPERTIES SUFFIX ".html")
  target_link_options(App PRIVATE -sASYNCIFY)
endif()
