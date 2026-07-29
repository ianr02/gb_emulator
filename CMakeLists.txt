cmake_minimum_required(VERSION 3.10)

project (GameBoyEmulator C)

set (CMAKE_C_STANDARD 17)
set (CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(main
    src/main.c
    core/emulator_core.c
    ppu/ppu.c
    cpu/cpu_instructions.c
)

target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(main PRIVATE SDL2::SDL2)
