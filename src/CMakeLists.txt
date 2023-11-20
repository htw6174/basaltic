cmake_minimum_required(VERSION 3.13)

project(basaltic LANGUAGES C CXX)

if(WIN32)
    add_executable(basaltic_engine WIN32 main.c basaltic_super.c basaltic_commandBuffer.c basaltic_window.c basaltic_editor_base.c bc_flecs_utils.c ${LIBS}/flecs.h ${LIBS}/flecs.c)
else(WIN32)#Unix
    add_executable(basaltic_engine main.c basaltic_super.c basaltic_commandBuffer.c basaltic_window.c basaltic_editor_base.c bc_flecs_utils.c ${LIBS}/flecs.h ${LIBS}/flecs.c)
endif(WIN32)

if (EMSCRIPTEN)
    set(HTW_STATIC YES)
    set(IMGUI_STATIC YES)
    # generate an html file that can be started with emrun (default for emscripten toolchain is .js)
    #set(CMAKE_EXECUTABLE_SUFFIX ".html")
    # TODO: only add --emrun on debug builds
    # TODO: find out if `-sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=1gb` is really needed, shouldn't use more than a half gig or so?
    # - Even on large maps, debug builds seem to stay under 200 MB
    target_compile_options(basaltic_engine PUBLIC "-pthread")
    set_target_properties(basaltic_engine PROPERTIES LINK_FLAGS "-sUSE_SDL=2 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3 -sINITIAL_MEMORY=256mb -sEXPORTED_RUNTIME_METHODS=cwrap -sMODULARIZE=1 -sEXPORT_NAME=\"createMyBasalticModule\" -pthread -sPTHREAD_POOL_SIZE=2 -sSTACK_SIZE=1mb --preload-file ${DATA_DIR}@data --emrun")
else (EMSCRIPTEN)

endif (EMSCRIPTEN)

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

## CIMGUI
# build cimgui with sdl+opengl backend
add_subdirectory(${LIBS}/cimgui ${CMAKE_BINARY_DIR}/cimgui)
set(IMGUI_BACKENDS "${LIBS}/cimgui/imgui/backends")

if(WIN32)
       list(APPEND IMGUI_LIBRARIES opengl32)
else(WIN32)#Unix
       list(APPEND IMGUI_LIBRARIES GL)
endif(WIN32)

#target_compile_definitions(cimgui PRIVATE RELEASE O2 CIMGUI_USE_OPENGL3 CIMGUI_USE_SDL2)
target_compile_definitions(cimgui PRIVATE CIMGUI_USE_OPENGL3 CIMGUI_USE_SDL2)
if (EMSCRIPTEN)
    target_compile_definitions(cimgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_ES3)
else (EMSCRIPTEN)
    target_compile_definitions(cimgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GL3W)
endif(EMSCRIPTEN)

list(APPEND IMGUI_SOURCES ${IMGUI_BACKENDS}/imgui_impl_opengl3.cpp)
list(APPEND IMGUI_SOURCES ${IMGUI_BACKENDS}/imgui_impl_sdl2.cpp)
set(IMGUI_SDL_LIBRARY SDL2::SDL2)

# Modify existing cimgui target to add SDL2 and OpenGL backend support
target_sources(cimgui PRIVATE ${IMGUI_SOURCES})
target_include_directories(cimgui PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(cimgui ${IMGUI_LIBRARIES} ${IMGUI_SDL_LIBRARY})
## /CIMGUI

add_subdirectory(${LIBS}/htw-libs ${CMAKE_BINARY_DIR}/bin)
add_subdirectory(model)
add_subdirectory(view)

# SDL2_INCLUDE_DIRS and SDL2_LIBRARIES are defined in /usr/lib/cmake/SDL2/SDL2Config.cmake (included with most SDL installations)
target_include_directories(basaltic_engine PRIVATE ${LIBS} include ${LIBS}/htw-libs/include ${LIBS}/cimgui model view ${SDL2_INCLUDE_DIRS})

# -lm flag is required for linking math libraries, including math.h (only on unix?)
target_link_libraries(basaltic_engine PRIVATE -lm htw basaltic_model basaltic_view cimgui SDL2::SDL2 SDL2::SDL2main)

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
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> ${IMGUI_LIB} "${CMAKE_BINARY_DIR}/bin")
endif(WIN32)

# install
install(TARGETS basaltic_engine RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/data DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/licenses DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/credits.md DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${IMGUI_LIB} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
if(WIN32)
    install(FILES $<TARGET_FILE:SDL2::SDL2> ${GLEW} ${MINGW_PATH}/bin/libstdc++-6.dll ${MINGW_PATH}/bin/libgcc_s_seh-1.dll ${MINGW_PATH}/bin/libwinpthread-1.dll DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
