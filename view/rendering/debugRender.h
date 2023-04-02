#ifndef DEBUGRENDER_H_INCLUDED
#define DEBUGRENDER_H_INCLUDED

#include "htw_core.h"
#include "htw_vulkan.h"
#include "basaltic_mesh.h"
#include "ccVector.h"

typedef struct {
    vec4 color;
    vec3 position;
    float size;
} bc_DebugInstanceData;

typedef struct {
    u32 maxInstanceCount;
    htw_PipelineHandle pipeline;
    htw_DescriptorSetLayout debugLayout;
    htw_DescriptorSet debugDescriptorSet;
    bc_Mesh instancedModel;
} bc_DebugRenderContext;

bc_DebugRenderContext *bc_createDebugRenderContext(htw_VkContext *vkContext, htw_BufferPool bufferPool, htw_DescriptorSetLayout perFrameLayout, htw_DescriptorSetLayout perPassLayout, u32 maxInstances);
bc_DebugInstanceData *bc_createDebugInstancePool(htw_VkContext *vkContext, size_t poolItemsCount);
void bc_updateDebugModel(htw_VkContext *vkContext, bc_DebugRenderContext *rc);
void bc_drawDebugInstances(htw_VkContext *vkContext, bc_DebugRenderContext *rc);

#endif // DEBUGRENDER_H_INCLUDED
