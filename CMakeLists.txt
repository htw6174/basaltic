cmake_minimum_required(VERSION 3.0)

project(kingdom LANGUAGES C)

add_executable(kingdom main.c)

# Command for simultanious development of supporting library
# Copy .c and .h files, and the CMakeLists files from the library directory into project directory
add_custom_command(TARGET kingdom PRE_BUILD COMMAND
    rsync -vrt --include "htw_libs" --include "*.[ch]" --include "CMakeLists.txt" --exclude "*" /home/htw/projects/c/htw_libs ${PROJECT_SOURCE_DIR}
    VERBATIM)

find_package(SDL2 REQUIRED)
# SDL2_INCLUDE_DIRS and SDL2_LIBRARIES are defined in /usr/lib/cmake/SDL2/SDL2Config.cmake (included with most SDL installations)
include_directories(${SDL2_INCLUDE_DIRS})

add_subdirectory(htw_libs)
target_link_libraries(kingdom PUBLIC htw_libs)
target_link_libraries(kingdom PUBLIC ${SDL2_LIBRARIES})
#add_subdirectory(quel_solaar)
#target_link_libraries(kingdom PUBLIC quel_solaar)

target_include_directories(kingdom PUBLIC "${PROJECT_SOURCE_DIR}/htw_libs")
#target_include_directories(kingdom PUBLIC "${PROJECT_SOURCE_DIR}/quel_solaar")

install(TARGETS kingdom RUNTIME DESTINATION bin)
