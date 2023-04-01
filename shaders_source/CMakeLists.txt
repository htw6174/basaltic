#set(SHADERS_SOURCE debug.frag debug.vert hexagon.frag hexagon.vert hexTerrain.frag hexTerrain.vert text.frag text.vert PARENT_SCOPE)

# Allows creating a custom target with all shader source files
file(GLOB SHADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/*.frag)

set(SHADER_SOURCE_FILES)
foreach(SHADER_FILE ${SHADER_FILES})
    # May be useful in the future for compiling shaders one at a time
#    get_filename_component(SHADER_FILE_NAME ${SHADER_FILE} NAME)
#    set(SPIRV_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_FILE_NAME}.spv)
#    add_custom_command(
#            OUTPUT ${SPIRV_FILE}
#            COMMAND glslangValidator -V ${SHADER_FILE} -o ${SPIRV_FILE}
#            DEPENDS ${SHADER_FILE}
#            COMMENT "Compiling ${SHADER_FILE} to ${SPIRV_FILE}"
#    )
    list(APPEND SHADER_SOURCE_FILES ${SHADER_FILE})
endforeach()

set(SHADERS_SOURCE ${SHADER_SOURCE_FILES} PARENT_SCOPE)
