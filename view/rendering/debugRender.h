#ifndef DEBUGRENDER_H_INCLUDED
#define DEBUGRENDER_H_INCLUDED

#include "htw_core.h"
#include "sokol_gfx.h"
#include "ccVector.h"

typedef struct {
    vec4 color;
    vec3 position;
    float size;
} bc_DebugInstanceData;

typedef struct {
    u32 maxInstanceCount;
    sg_pipeline pipeline;
    bc_DebugInstanceData *instanceData;
    // htw_DescriptorSetLayout debugLayout;
    // htw_DescriptorSet debugDescriptorSet;
    // bc_Mesh instancedModel;
} bc_DebugRenderContext;

bc_DebugRenderContext *bc_createDebugRenderContext(sg_shader_uniform_block_desc perFrameUniforms, u32 maxInstances);
bc_DebugInstanceData *bc_createDebugInstancePool(size_t poolItemsCount);
void bc_updateDebugModel(bc_DebugRenderContext *rc);
void bc_drawDebugInstances(bc_DebugRenderContext *rc);

#endif // DEBUGRENDER_H_INCLUDED
