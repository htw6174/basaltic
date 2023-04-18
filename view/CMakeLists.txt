cmake_minimum_required(VERSION 3.13)

project(basaltic LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

add_library(basaltic_view basaltic_view.c basaltic_interaction.c basaltic_editor.c basaltic_uiState.c)

target_include_directories(basaltic_view PRIVATE include sokol ecs)
add_subdirectory(ecs)

add_subdirectory(shaders)
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders")
add_custom_target(shaders ALL
        COMMENT "Building shaders"
        DEPENDS ${SHADERS_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADERS_SOURCE} ${PROJECT_BINARY_DIR}/shaders
        VERBATIM
        )
# Make sure the shaders target is always built before this module
add_dependencies(basaltic_view shaders)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# TODO: any way to avoid including model/ecs explicitly?
target_include_directories(basaltic_view PRIVATE ../htw-libs ../include ../cimgui ../model ../model/ecs ${SDL2_INCLUDE_DIRS} ${OPENGL_gl_LIBRARY})

target_link_libraries(basaltic_view PRIVATE htw SDL2::SDL2 OpenGL::OpenGL)
