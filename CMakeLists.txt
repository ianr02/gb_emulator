cmake_minimum_required(VERSION 3.10)

project (GameBoyEmulator C)

set (CMAKE_C_STANDARD 17)
set (CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(emulator
    src/main.c
    core/emulator_core.c
    core/bus.c
    core/dma.c
    core/oam.c
    ppu/ppu.c
    ppu/ppu_memory.c
    cpu/cpu_instructions.c
    apu/apu.c
)

target_include_directories(emulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(emulator PRIVATE SDL2::SDL2)
