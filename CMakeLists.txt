cmake_minimum_required(VERSION 3.0)

project(kingdom LANGUAGES C)

add_executable(kingdom main.c kingdom_logic.c kingdom_logicInputState.c kingdom_uiState.c kingdom_window.c kingdom_interaction.c)

# Command for simultanious development of supporting library
# Copy .c and .h files, and the CMakeLists files from the library directory into project directory
# NB: this command only runs if the main project is rebuilt. To force this, simply touch a source file before running cmake
add_custom_command(TARGET kingdom PRE_BUILD COMMAND
    rsync -vrt --include "htw_libs" --include "*.[ch]" --include "CMakeLists.txt" --exclude "*" /home/htw/projects/c/htw_libs ${PROJECT_SOURCE_DIR}
    VERBATIM)

# find_package should locate /usr/lib/cmake/SDL2/SDL2Config.cmake and /usr/share/cmake/Modules/FindVulkan.cmake (or equivalent)
# .cmake files under /usr/lib/cmake can be located this way to load variables
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
# SDL2_INCLUDE_DIRS and SDL2_LIBRARIES are defined in /usr/lib/cmake/SDL2/SDL2Config.cmake (included with most SDL installations)
include_directories(${SDL2_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS})

add_subdirectory(htw_libs)
# -lm flag is required for linking math libraries, including math.h (only on unix?)
target_link_libraries(kingdom PUBLIC -lm htw_libs ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES})

target_include_directories(kingdom PUBLIC "${PROJECT_SOURCE_DIR}/htw_libs")

install(TARGETS kingdom RUNTIME DESTINATION bin)
