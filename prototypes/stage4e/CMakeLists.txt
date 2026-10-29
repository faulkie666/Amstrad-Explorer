cmake_minimum_required(VERSION 3.20)
project(cpc_stage4e_fast VERSION 0.1 LANGUAGES C CXX)

if(MSVC)
    message(FATAL_ERROR "Stage 4E uses the reviewed cpu-z80 GNU computed-goto core; use Clang or GCC")
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

add_executable(cpc_stage4e_fast
    stage4e_fast.cpp
    ../stage4d/fast_z80_adapter.c
    "${cpu_z80_SOURCE_DIR}/cpu-z80/cpu/z80.c")
set_source_files_properties(
    ../stage4d/fast_z80_adapter.c
    "${cpu_z80_SOURCE_DIR}/cpu-z80/cpu/z80.c"
    PROPERTIES COMPILE_DEFINITIONS "z80_reset=fastcore_z80_reset")
target_include_directories(cpc_stage4e_fast PRIVATE
    . ../stage4d ../.. ../../third_party/chips "${cpu_z80_SOURCE_DIR}/cpu-z80")
target_compile_definitions(cpc_stage4e_fast PRIVATE CHIPS_NO_ASSERT)
target_compile_options(cpc_stage4e_fast PRIVATE -O3 -Wall -Wextra -Wpedantic)
if(WIN32)
    target_link_options(cpc_stage4e_fast PRIVATE "LINKER:/STACK:8388608")
endif()
