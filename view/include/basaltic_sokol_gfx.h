/* Extensions to sokol_gfx for the Basaltic engine */
#ifndef BASALTIC_SOKOL_GFX_H_INCLUDED
#define BASALTIC_SOKOL_GFX_H_INCLUDED

#include "sokol_gfx.h"
#include "ccVector.h"

// TODO: issue with this approach is that it prevents the model matrix uniform block from using any other uniforms (so only 4/16 slots are used). Bad if I need many uniforms
static void bc_drawWrapInstances(int base_element, int num_elements, int num_instances, int modelMatrixUniformBlockIndex, vec3 position, const vec3 *offsets);

static void bc_drawWrapInstances(int base_element, int num_elements, int num_instances, int modelMatrixUniformBlockIndex, vec3 position, const vec3 *offsets) {
    static mat4x4 model;
    for (int i = 0; i < 4; i++) {
        mat4x4SetTranslation(model, vec3Add(position, offsets[i]));
        sg_apply_uniforms(SG_SHADERSTAGE_VS, modelMatrixUniformBlockIndex, &SG_RANGE(model));
        sg_draw(base_element, num_elements, num_instances);
    }
}

#endif // BASALTIC_SOKOL_GFX_H_INCLUDED
