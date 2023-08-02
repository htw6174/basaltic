/* Extensions to sokol_gfx for the Basaltic engine */
#ifndef BASALTIC_SOKOL_GFX_H_INCLUDED
#define BASALTIC_SOKOL_GFX_H_INCLUDED

#include "sokol_gfx.h"
#include "ccVector.h"

void bc_glCheck(void);

void bc_sg_setup(void);

uint32_t bc_sg_getImageGluint(sg_image image);

// TODO: issue with this approach is that it prevents the model matrix uniform block from using any other uniforms (so only 4/16 slots are used). Bad if I need many uniforms
void bc_drawWrapInstances(int base_element, int num_elements, int num_instances, int modelMatrixUniformBlockIndex, vec3 position, const vec3 *offsets);

sg_image bc_loadImage(const char *filename);

/**
 * @brief Loads the specified shader source files and attempts to create an sg_shader from them.
 *
 * @param vertSourcePath p_vertSourcePath:...
 * @param fragSourcePath p_fragSourcePath:...
 * @param shaderDescription p_shaderDescription:...
 * @param out_shader p_out_shader:...
 * @return 0 on success; -1 on unspecified error (nothing is written to out_shader)
 */
int bc_loadShader(const char *vertSourcePath, const char *fragSourcePath, const sg_shader_desc *shaderDescription, sg_shader *out_shader);

/**
 * @brief Creates a pipeline from the provided description and optional shader.
 *
 * @param pipelineDescription The .shader field may be uninitialized if shader is provided
 * @param shader If not NULL and shader is valid, will use this shader instead of the one provided in pipelineDescription
 * @param out_pipeline p_out_pipeline:...
 * @return 0 on success; -1 on pipeline creation error; -2 if no valid shader provided
 */
int bc_createPipeline(const sg_pipeline_desc *pipelineDescription, const sg_shader *shader, sg_pipeline *out_pipeline);

#endif // BASALTIC_SOKOL_GFX_H_INCLUDED
