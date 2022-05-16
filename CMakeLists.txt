cmake_minimum_required(VERSION 3.0)

project(kingdom LANGUAGES C)

add_executable(kingdom main.c)

add_custom_command(TARGET kingdom PRE_BUILD COMMAND
    rsync -vrt --include "htw_libs" --include "*.[ch]" --include "CMakeLists.txt" --exclude "*" /home/htw/projects/c/htw_libs ${PROJECT_SOURCE_DIR}
    VERBATIM)

add_subdirectory(htw_libs)
target_link_libraries(kingdom PUBLIC htw_libs)

target_include_directories(kingdom PUBLIC "${PROJECT_SOURCE_DIR}/htw_libs")

install(TARGETS kingdom RUNTIME DESTINATION bin)
