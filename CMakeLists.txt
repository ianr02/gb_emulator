cmake_minimum_required(VERSION 3.10)

project (GameBoyEmulator C)

set (CMAKE_C_STANDARD 17)
set (CMAKE_C_STANDARD_REQUIRED ON)

find_package(X11 REQUIRED)

add_executable(emulator emulator.c)

target_link_libraries(emulator PRIVATE X11::X11)