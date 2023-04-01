cmake_minimum_required(VERSION 3.13)

project(basaltic LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

add_library(basaltic_view basaltic_view.c basaltic_render.c basaltic_interaction.c basaltic_editor.c basaltic_uiState.c)

target_include_directories(basaltic_view PRIVATE ${CMAKE_CURRENT_LIST_DIR} rendering)
add_subdirectory(rendering)

find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
#find_package(Freetype REQUIRED)

# TODO: any way to avoid including model/ecs explicitly?
target_include_directories(basaltic_view PRIVATE .. ../htw-libs ../include ../model ../model/ecs ${SDL2_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS})

# NOTE: no need to link libraries in this cmake file, because linking is already done in the root CMakeLists.txt
#target_link_libraries(${SDL2_LIBRARIES} ${Vulkan_LIBRARIES})
