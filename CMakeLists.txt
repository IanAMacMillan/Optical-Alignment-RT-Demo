cmake_minimum_required(VERSION 3.20)

project(optical_alignment_rt_demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(FREERTOS_KERNEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/FreeRTOS-Kernel)
set(FREERTOS_PORT_DIR ${FREERTOS_KERNEL_DIR}/portable/ThirdParty/GCC/Posix)

add_executable(optical_alignment_rt_demo
    src/freertos_app.c
    ${FREERTOS_KERNEL_DIR}/tasks.c
    ${FREERTOS_KERNEL_DIR}/queue.c
    ${FREERTOS_KERNEL_DIR}/list.c
    ${FREERTOS_KERNEL_DIR}/timers.c
    ${FREERTOS_KERNEL_DIR}/event_groups.c
    ${FREERTOS_KERNEL_DIR}/stream_buffer.c
    ${FREERTOS_PORT_DIR}/port.c
    ${FREERTOS_PORT_DIR}/utils/wait_for_event.c
    ${FREERTOS_KERNEL_DIR}/portable/MemMang/heap_4.c
)

target_include_directories(optical_alignment_rt_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FREERTOS_KERNEL_DIR}/include
    ${FREERTOS_PORT_DIR}
)

target_compile_options(optical_alignment_rt_demo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(optical_alignment_rt_demo PRIVATE
    pthread
)
