cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(pico-helloworld)

pico_sdk_init()

add_executable(pico-helloworld
    src/main.c
)

pico_add_extra_outputs(pico-helloworld)

target_link_libraries(pico-helloworld pico_stdlib)
