cmake_minimum_required(VERSION 3.13)

cmake_policy(SET CMP0076 NEW)

project(basaltic LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
#add_compile_definitions($<$<CONFIG:Debug>:FLECS_SANITIZE>)

add_executable(basaltic WIN32 main.c basaltic_super.c basaltic_commandBuffer.c basaltic_window.c basaltic_editor_base.c)

# TODO: not needed right now, but should eventually find a way to only compile a single shader source if .spv is outdated
# Create a custom target to compile shader source files to SPIR-V
add_subdirectory(shaders_source)
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders_bin")
add_custom_target(shaders ALL
        COMMENT "Compiling shaders: ${SHADERS_SOURCE}"
        DEPENDS ${SHADERS_SOURCE}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/shaders_bin
        COMMAND glslc -c ${SHADERS_SOURCE}
        VERBATIM
        )
# Make sure the shaders target is always built before the main executable
add_dependencies(basaltic shaders)

# find_package should locate /usr/lib/cmake/SDL2/SDL2Config.cmake and /usr/share/cmake/Modules/FindVulkan.cmake (or equivalent)
# .cmake files under /usr/lib/cmake can be located this way to load variables
if(NOT DEFINED SDL2_DIR OR SDL2_DIR STREQUAL "SDL2_DIR-NOTFOUND")
    # *NOTE*: Change this to match your SDL cmake config directory
    set(SDL2_DIR "D:/Programming/sdk/SDL2-2.26.4/x86_64-w64-mingw32/lib/cmake/SDL2")
endif()
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

# TODO: build cimgui with sdl+vulkan backend, optionally static link. For now will use prebuilt libs
#set(IMGUI_STATIC "yes")
#add_subdirectory(cimgui)
#set(BACKENDS_FOLDER "cimgui/imgui/backends/")
#list(APPEND IMGUI_LIBRARIES Vulkan::Vulkan)
#list(APPEND IMGUI_SOURCES ${BACKENDS_FOLDER}imgui_impl_vulkan.cpp)
#list(APPEND IMGUI_SOURCES ${BACKENDS_FOLDER}imgui_impl_sdl2.cpp)
#set(IMGUI_SDL_LIBRARY SDL2::SDL2)
#add_library(cimgui_sdl STATIC ${IMGUI_SOURCES})
#target_link_libraries(cimgui_sdl ${IMGUI_LIBRARIES} ${IMGUI_SDL_LIBRARY})
#target_compile_definitions(basaltic PUBLIC -DCIMGUI_USE_VULKAN -DCIMGUI_USE_SDL2)

set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/cimgui)
if(WIN32)
    set(IMGUI_LIB ${IMGUI_DIR}/cimgui.dll ${IMGUI_DIR}/cimgui_sdl.dll)
else()
    set(IMGUI_LIB ${IMGUI_DIR}/cimgui.so ${IMGUI_DIR}/libcimgui_sdl.so)
endif()

add_subdirectory(htw-libs)
add_subdirectory(model)
add_subdirectory(view)

# Potential to use this for static linking in the future, unneeded for now
# Set default standard libraries
#if(WIN32)
#    set(CMAKE_C_STANDARD_LIBRARIES "-lkernel32 -luser32 -lgdi32 -lwinspool -lcomdlg32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm -lversion -static-libgcc -static-libstdc++")
#endif()
#if(WIN32)
#    target_link_options(basaltic PRIVATE -static -static-libgcc -static-libstdc++)
#endif()

# SDL2_INCLUDE_DIRS and SDL2_LIBRARIES are defined in /usr/lib/cmake/SDL2/SDL2Config.cmake (included with most SDL installations)
target_include_directories(basaltic PRIVATE include/ htw-libs/ model/ view/ cimgui/ ${SDL2_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS})

# -lm flag is required for linking math libraries, including math.h (only on unix?)
target_link_libraries(basaltic PRIVATE -lm htw basaltic_model basaltic_view SDL2::SDL2 SDL2::SDL2main Vulkan::Vulkan ${IMGUI_LIB})

# Set output directories
set_target_properties(basaltic htw basaltic_model basaltic_view
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )

if(WIN32)
    add_custom_command(TARGET basaltic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> ${IMGUI_LIB} ${CMAKE_BINARY_DIR}/bin)
endif()

# install
install(TARGETS basaltic RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${PROJECT_BINARY_DIR}/shaders_bin DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/licenses DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/credits.md DESTINATION ${CMAKE_INSTALL_PREFIX})
# Install imgui.ini; may want to keep a separate development and release version?
install(FILES ${PROJECT_BINARY_DIR}/imgui.ini DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${IMGUI_LIB} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
if(WIN32)
    # *NOTE*: change this to match your MinGW install / IDE
    set(MINGW_PATH "C:/Program Files/JetBrains/CLion 2023.1/bin/mingw")
    install(FILES $<TARGET_FILE:SDL2::SDL2> ${MINGW_PATH}/bin/libstdc++-6.dll ${MINGW_PATH}/bin/libgcc_s_seh-1.dll ${MINGW_PATH}/bin/libwinpthread-1.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
