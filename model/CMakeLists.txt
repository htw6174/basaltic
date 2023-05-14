cmake_minimum_required(VERSION 3.13)

project(basaltic LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

add_library(basaltic_model basaltic_model.c basaltic_logic.c basaltic_worldGen.c)

find_package(SDL2 REQUIRED)

target_include_directories(basaltic_model PUBLIC ../htw-libs ../include ecs ${SDL2_INCLUDE_DIRS})
add_subdirectory(ecs)

target_link_libraries(basaltic_model PRIVATE htw SDL2::SDL2)
