cmake_minimum_required(VERSION 3.12)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(read_max30003 C CXX ASM)

set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(read_max30003
    read_max30003.cpp
)

# pull in common dependencies
target_link_libraries(read_max30003 pico_stdlib hardware_uart hardware_spi pico_multicore)
 
# create map/bin/hex file etc.
pico_add_extra_outputs(read_max30003)

pico_enable_stdio_uart(read_max30003 1)