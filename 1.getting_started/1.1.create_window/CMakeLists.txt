cmake_minimum_required(VERSION 3.0...3.25)
project(
	LearnWebGPU
	VERSION 0.1.0
	LANGUAGES CXX C
)

# Include the file that defines target_treat_all_warnings_as_errors
# (I do this because I reuse this function across many projects and
# want to keep this main file easy to read).
include(utils.cmake)

# Create the App executable target with one source file
add_executable(App
	main.cpp
)

# Use C++17
set_target_properties(App PROPERTIES CXX_STANDARD 17)

# Turn all warnings on and treat them as errors so that are not
# tempted to ignore them (See utils.cmake for details).
target_treat_all_warnings_as_errors(App)

include(FetchContent)
FetchContent_Declare(
  webgpu
  GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
  GIT_TAG        main
)
FetchContent_MakeAvailable(webgpu)

target_link_libraries(App PRIVATE webgpu)

target_copy_webgpu_binaries(App)