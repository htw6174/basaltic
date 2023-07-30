cmake_minimum_required(VERSION 3.13)

project(basaltic LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

add_library(basaltic_view basaltic_view.c basaltic_interaction.c basaltic_editor.c basaltic_uiState.c basaltic_sokol_gfx.c)

target_include_directories(basaltic_view PRIVATE ../include include sokol ecs)
add_subdirectory(ecs)

find_package(SDL2 REQUIRED)

find_package(OpenGL REQUIRED)

# TODO: any way to avoid including model/ecs explicitly?
target_include_directories(basaltic_view PRIVATE ${LIBS} ${LIBS}/htw-libs ${LIBS}/cimgui ${LIBS}/sokol ../model ../model/ecs ${SDL2_INCLUDE_DIRS} ${OPENGL_gl_LIBRARY})

target_link_libraries(basaltic_view PRIVATE htw SDL2::SDL2 OpenGL::GL)