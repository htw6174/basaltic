/* Extensions to sokol_gfx for the Basaltic engine */
#ifndef BASALTIC_SOKOL_GFX_H_INCLUDED
#define BASALTIC_SOKOL_GFX_H_INCLUDED

#include "sokol_gfx.h"
#include "ccVector.h"

// TODO: issue with this approach is that it prevents the model matrix uniform block from using any other uniforms (so only 4/16 slots are used). Bad if I need many uniforms
void bc_drawWrapInstances(int base_element, int num_elements, int num_instances, int modelMatrixUniformBlockIndex, vec3 position, const vec3 *offsets);

sg_image bc_loadImage(const char *filename);

#endif // BASALTIC_SOKOL_GFX_H_INCLUDED
