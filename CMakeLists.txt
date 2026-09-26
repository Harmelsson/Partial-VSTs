# Partial-VSTs — top-level build.
#
# One CMake project builds every plugin in plugins/, plus the headless tests in tools/.
# JUCE is downloaded at configure time (pinned version), so nobody has to install it.
#
#   Linux (agent container):  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
#   Windows (GitHub Actions): see .github/workflows/build.yml
cmake_minimum_required(VERSION 3.22)
project(PartialVSTs VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: link the C++ runtime statically, so the user never has to install a
# "Visual C++ Redistributable" for a plugin to load in FL Studio.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---- Build identity, shown in every plugin's UI -----------------------------------------
# The user checks this label to confirm FL Studio loaded the newest build.
# PVST_BUILD_NUMBER is the GitHub Actions run number in CI ("local" otherwise).
set(PVST_BUILD_NUMBER "local" CACHE STRING "Build number shown in plugin UIs")
execute_process(COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE PVST_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT PVST_GIT_HASH)
    set(PVST_GIT_HASH "nogit")
endif()

# ---- Code shared by all plugins (header-only) -------------------------------------------
add_library(pvst_shared INTERFACE)
target_include_directories(pvst_shared INTERFACE ${CMAKE_SOURCE_DIR}/shared)
target_compile_definitions(pvst_shared INTERFACE
    PVST_BUILD_NUMBER="${PVST_BUILD_NUMBER}"
    PVST_GIT_HASH="${PVST_GIT_HASH}")

add_subdirectory(plugins/Seed)

enable_testing()
add_subdirectory(tools)
