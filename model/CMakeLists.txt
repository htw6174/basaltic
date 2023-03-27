cmake_minimum_required(VERSION 3.13)

project(basaltic LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

add_library(basaltic_model basaltic_model.c basaltic_logic.c basaltic_worldGen.c)

target_include_directories(basaltic_model PUBLIC ../htw-libs ../include . ecs)
add_subdirectory(ecs)

# NOTE: no need to link libraries in this cmake file, because linking is already done in the root CMakeLists.txt
