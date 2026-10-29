cmake_minimum_required(VERSION 3.20)
project(cpc_stage4d_prototype VERSION 0.1 LANGUAGES C CXX)

if(MSVC)
    message(FATAL_ERROR
        "The reviewed cpu-z80 revision uses GNU computed-goto syntax. "
        "Build this host-only prototype with Clang or GCC; production firmware is unchanged.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    cpu_z80
    GIT_REPOSITORY https://github.com/michalhucik/z80-mz800.git
    GIT_TAG 687840df5b449e3b8ee31bf4cbc20f4ec6cf6d59
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(cpu_z80)

add_executable(cpc_stage4d_prototype
    stage4d_prototype.cpp
    fast_z80_adapter.c
    "${cpu_z80_SOURCE_DIR}/cpu-z80/cpu/z80.c")
# chips also exports z80_reset(). Keep both engines linkable in the isolated
# differential executable without modifying either upstream implementation.
set_source_files_properties(
    fast_z80_adapter.c
    "${cpu_z80_SOURCE_DIR}/cpu-z80/cpu/z80.c"
    PROPERTIES COMPILE_DEFINITIONS "z80_reset=fastcore_z80_reset")
target_include_directories(cpc_stage4d_prototype PRIVATE
    .
    ../..
    ../../third_party/chips
    "${cpu_z80_SOURCE_DIR}/cpu-z80")
target_compile_definitions(cpc_stage4d_prototype PRIVATE CHIPS_NO_ASSERT)
if(WIN32)
    # The differential executable deliberately keeps four independent CPC
    # states alive while benchmarking. Production firmware does not use this.
    target_link_options(cpc_stage4d_prototype PRIVATE "LINKER:/STACK:8388608")
endif()

target_compile_options(cpc_stage4d_prototype PRIVATE -O3 -Wall -Wextra -Wpedantic)

