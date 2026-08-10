cmake_minimum_required(VERSION 3.15)
project(bitstream_decoder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ter_parser
    src/ter_parser.cpp
)

if (MSVC)
    target_compile_options(ter_parser PRIVATE /W4 /permissive-)
else()
    target_compile_options(ter_parser PRIVATE -Wall -Wextra -Wpedantic)
endif()
