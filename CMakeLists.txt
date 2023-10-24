cmake_minimum_required(VERSION 3.13)

cmake_policy(SET CMP0076 NEW)

project(basaltic LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
add_compile_definitions($<$<CONFIG:Debug>:FLECS_SANITIZE>)
#add_compile_definitions("CIMGUI_USE_OPENGL3")

set(LIBS ${PROJECT_SOURCE_DIR}/libs)

add_subdirectory(src)
