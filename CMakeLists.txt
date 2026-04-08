cmake_minimum_required(VERSION 3.20)

project(optical_alignment_rt_demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(optical_alignment_rt_demo
    src/main.c
)