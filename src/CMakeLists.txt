cmake_minimum_required(VERSION 3.13)

project(basaltic LANGUAGES C)

add_executable(basaltic_engine WIN32 main.c basaltic_super.c basaltic_commandBuffer.c basaltic_window.c basaltic_editor_base.c bc_flecs_utils.c ${LIBS}/flecs.h ${LIBS}/flecs.c)

#if(WIN32)
#    add_library(GLEW SHARED IMPORTED)
#    # *NOTE*: Change this to your copy of glew
#    set_target_properties(GLEW PROPERTIES IMPORTED_LOCATION ${MINGW_PATH}/bin/glew32.dll)
#endif()

# find_package should locate /usr/lib/cmake/SDL2/SDL2Config.cmake
# .cmake files under /usr/lib/cmake can be located this way to load variables
if(NOT DEFINED SDL2_DIR OR SDL2_DIR STREQUAL "SDL2_DIR-NOTFOUND")
    # *NOTE*: Change this to match your SDL cmake config directory
    set(SDL2_DIR "D:/Programming/sdk/SDL2-2.26.4/x86_64-w64-mingw32/lib/cmake/SDL2")
endif()
find_package(SDL2 REQUIRED)

# TODO: build cimgui with sdl+opengl backend, optionally static link. For now will use prebuilt libs
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

set(IMGUI_DIR ${LIBS}/cimgui)
if(WIN32)
    set(IMGUI_LIB ${IMGUI_DIR}/cimgui.dll ${IMGUI_DIR}/cimgui_sdl.dll)
else()
    set(IMGUI_LIB ${IMGUI_DIR}/cimgui.so ${IMGUI_DIR}/libcimgui_sdl.so)
endif()

add_subdirectory(${LIBS}/htw-libs ${CMAKE_BINARY_DIR}/bin)
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
target_include_directories(basaltic_engine PRIVATE ${LIBS} include ${LIBS}/htw-libs/include ${LIBS}/cimgui model view ${SDL2_INCLUDE_DIRS})

# -lm flag is required for linking math libraries, including math.h (only on unix?)
target_link_libraries(basaltic_engine PRIVATE -lm htw basaltic_model basaltic_view SDL2::SDL2 SDL2::SDL2main ${IMGUI_LIB})

# Set output directories
set_target_properties(basaltic_engine htw basaltic_model basaltic_view
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )


if(WIN32)
    # *NOTE*: change this to match your MinGW install / IDE
    set(MINGW_PATH "D:/Programs/mingw64")
    #find_library(GLEW glew32 REQUIRED)
    set(GLEW ${MINGW_PATH}/bin/glew32.dll)
    # link glew for opengl, and winsock for Flecs' REST API
    target_link_libraries(basaltic_view PRIVATE ${GLEW})
    target_link_libraries(basaltic_engine PRIVATE ws2_32)
    # on Windows, need to include some other binaries with .exe
    add_custom_command(TARGET basaltic_engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> ${IMGUI_LIB} ${CMAKE_BINARY_DIR}/bin)
endif()

# install
install(TARGETS basaltic_engine RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${PROJECT_BINARY_DIR}/view/shaders DESTINATION ${CMAKE_INSTALL_PREFIX}/view)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/licenses DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/credits.md DESTINATION ${CMAKE_INSTALL_PREFIX})
# Install imgui.ini; may want to keep a separate development and release version?
install(FILES ${PROJECT_BINARY_DIR}/imgui.ini DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${IMGUI_LIB} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
if(WIN32)
    install(FILES $<TARGET_FILE:SDL2::SDL2> ${GLEW} ${MINGW_PATH}/bin/libstdc++-6.dll ${MINGW_PATH}/bin/libgcc_s_seh-1.dll ${MINGW_PATH}/bin/libwinpthread-1.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
