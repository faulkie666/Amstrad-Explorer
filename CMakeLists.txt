cmake_minimum_required(VERSION 3.20)

option(CPCEXPLORER_BUILD_FIRMWARE "Build RP2350 firmware instead of the host test" OFF)

if(CPCEXPLORER_BUILD_FIRMWARE)
    set(PICO_BOARD pimoroni_explorer CACHE STRING "Explorer RP2350B board")
    set(PICO_PLATFORM rp2350 CACHE STRING "RP2350 ARM platform")
    list(APPEND PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/src/display")
    if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
    endif()
    include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
endif()

project(cpc_explorer VERSION 0.1.0 LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CPC_ROM "${CMAKE_CURRENT_LIST_DIR}/roms/cpc464.rom" CACHE FILEPATH "Original 32 KiB CPC 464 ROM")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_ROM "${GENERATED_DIR}/cpc_rom.h")
add_custom_command(
    OUTPUT "${GENERATED_ROM}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/embed_rom.py" "${CPC_ROM}" "${GENERATED_ROM}"
    DEPENDS "${CPC_ROM}" "${CMAKE_CURRENT_LIST_DIR}/tools/embed_rom.py"
    VERBATIM)

add_library(cpc_core STATIC src/cpc_core.cpp "${GENERATED_ROM}")
target_include_directories(cpc_core PUBLIC src PRIVATE "${GENERATED_DIR}" third_party/chips)
target_compile_definitions(cpc_core PRIVATE CHIPS_NO_ASSERT)

if(CPCEXPLORER_BUILD_FIRMWARE)
    pico_sdk_init()
    add_executable(cpc_explorer
        src/main.cpp
        src/display/st7789.c
        src/display/st7789_parallel.pio)
    pico_generate_pio_header(cpc_explorer "${CMAKE_CURRENT_LIST_DIR}/src/display/st7789_parallel.pio")
    target_include_directories(cpc_explorer PRIVATE src src/display)
    target_link_libraries(cpc_explorer PRIVATE
        cpc_core pico_stdlib hardware_clocks hardware_dma hardware_gpio hardware_pio hardware_pwm)
    pico_enable_stdio_usb(cpc_explorer 1)
    pico_enable_stdio_uart(cpc_explorer 0)
    pico_set_program_name(cpc_explorer "CPC Explorer")
    pico_set_program_version(cpc_explorer "0.1.0-stage1b")
    pico_add_extra_outputs(cpc_explorer)
    target_compile_options(cpc_explorer PRIVATE -Wall -Wextra -Wpedantic)
else()
    add_executable(cpc_host_boot_test tests/host_boot_test.cpp)
    target_link_libraries(cpc_host_boot_test PRIVATE cpc_core)
    if(MSVC)
        target_compile_options(cpc_core PRIVATE /W4 /permissive-)
        target_compile_options(cpc_host_boot_test PRIVATE /W4 /permissive-)
    else()
        target_compile_options(cpc_core PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(cpc_host_boot_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_testing()
    add_test(NAME cpc464_rom_boot COMMAND cpc_host_boot_test)
endif()
