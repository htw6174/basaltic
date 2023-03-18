cmake_minimum_required(VERSION 3.2)

project(basaltic LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

add_library(basaltic_model basaltic_model.c basaltic_logic.c basaltic_worldGen.c basaltic_characters.c flecs.c)

target_include_directories(basaltic_model PRIVATE ../htw-libs ../include)

# NOTE: no need to link libraries in this cmake file, because linking is already done in the root CMakeLists.txt
