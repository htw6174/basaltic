cmake_minimum_required(VERSION 3.2)

project(basaltic LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

add_executable(basaltic main.c basaltic_logic.c basaltic_logicInputState.c basaltic_uiState.c basaltic_window.c basaltic_interaction.c basaltic_characters.c basaltic_editor.c)

# Command for simultanious development of supporting library
# Copy .c and .h files, and the CMakeLists files from the library directory into project directory
# NB: this command only runs if the main project is rebuilt. To force this, simply touch a source file before running cmake
#add_custom_command(TARGET basaltic PRE_BUILD COMMAND
#    rsync -vrt --include "htw_libs" --include "*.[ch]" --include "CMakeLists.txt" --exclude "*" /home/htw/projects/c/htw_libs ${PROJECT_SOURCE_DIR}
#    VERBATIM)

# copy to build directory
# TODO: this will only run when cmake generates the makefile. Figure out how to create a custom target that includes all the files that would be copied here, and move them to the build directory with a CMAKE_COMMAND instruction
file(COPY resources licenses credits.txt README.md DESTINATION .)
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders")

# precompile shaders to SPIR-V
# TODO: not needed right now, but should eventually find a way to only compile shader source if .spv is outdated
# TODO: may fix the above issue by depending this command on a custom target instead of the main target
file(GLOB SHADERS_SOURCE "${PROJECT_SOURCE_DIR}/shaders/*.vert" "${PROJECT_SOURCE_DIR}/shaders/*.frag")
add_custom_command(TARGET basaltic PRE_BUILD COMMENT "Compiling shaders..." COMMAND
    glslc -c ${SHADERS_SOURCE}
    VERBATIM WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/shaders")

# find_package should locate /usr/lib/cmake/SDL2/SDL2Config.cmake and /usr/share/cmake/Modules/FindVulkan.cmake (or equivalent)
# .cmake files under /usr/lib/cmake can be located this way to load variables
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Freetype REQUIRED)
# SDL2_INCLUDE_DIRS and SDL2_LIBRARIES are defined in /usr/lib/cmake/SDL2/SDL2Config.cmake (included with most SDL installations)
include_directories(${SDL2_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2} cimgui)

#add_subdirectory(htw_libs)
include_directories(../htw_libs)
# -lm flag is required for linking math libraries, including math.h (only on unix?)
target_link_libraries(basaltic PUBLIC -lm /home/htw/projects/c/htw_libs/$<$<CONFIG:Debug>:build>$<$<CONFIG:Release>:release>/libhtw.so ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} ${FREETYPE_LIBRARIES} "${PROJECT_SOURCE_DIR}/cimgui/cimgui.so" "${PROJECT_SOURCE_DIR}/cimgui/libcimgui_sdl.so")

#target_include_directories(basaltic PUBLIC "${PROJECT_SOURCE_DIR}/htw_libs")

# TODO: add install commands to handle data files
install(TARGETS basaltic RUNTIME DESTINATION bin)
