cmake_minimum_required(VERSION 3.2)

project(kingdom LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

add_executable(kingdom main.c kingdom_logic.c kingdom_logicInputState.c kingdom_uiState.c kingdom_window.c kingdom_interaction.c)

# Command for simultanious development of supporting library
# Copy .c and .h files, and the CMakeLists files from the library directory into project directory
# NB: this command only runs if the main project is rebuilt. To force this, simply touch a source file before running cmake
#add_custom_command(TARGET kingdom PRE_BUILD COMMAND
#    rsync -vrt --include "htw_libs" --include "*.[ch]" --include "CMakeLists.txt" --exclude "*" /home/htw/projects/c/htw_libs ${PROJECT_SOURCE_DIR}
#    VERBATIM)

# precompile shaders to SPIR-V
# FIXME: no shaders get compiled, but no error from glslc
file(GLOB shaders "${PROJECT_SOURCE_DIR}/shaders/*.vert" "${PROJECT_SOURCE_DIR}/shaders/*.frag")
add_custom_command(TARGET kingdom PRE_BUILD COMMENT "Compiling shaders..." COMMAND
    glslc -c ${shaders}
    VERBATIM )
# copy to build directory FIXME: why no copy cmake???? only works when makefile is regenerated?
file(COPY shaders DESTINATION .)
file(COPY resources DESTINATION .)

# find_package should locate /usr/lib/cmake/SDL2/SDL2Config.cmake and /usr/share/cmake/Modules/FindVulkan.cmake (or equivalent)
# .cmake files under /usr/lib/cmake can be located this way to load variables
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Freetype REQUIRED)
# SDL2_INCLUDE_DIRS and SDL2_LIBRARIES are defined in /usr/lib/cmake/SDL2/SDL2Config.cmake (included with most SDL installations)
include_directories(${SDL2_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIR_ft2build} ${FREETYPE_INCLUDE_DIR_freetype2})

#add_subdirectory(htw_libs)
include_directories(../htw_libs)
# -lm flag is required for linking math libraries, including math.h (only on unix?)
target_link_libraries(kingdom PUBLIC -lm /home/htw/projects/c/htw_libs/$<$<CONFIG:Debug>:build>$<$<CONFIG:Release>:release>/libhtw.so ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} ${FREETYPE_LIBRARIES})

#target_include_directories(kingdom PUBLIC "${PROJECT_SOURCE_DIR}/htw_libs")

install(TARGETS kingdom RUNTIME DESTINATION bin)
