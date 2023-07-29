#include "debugRender.h"
#include "htw_core.h"
#include "htw_vulkan.h"
#include "basaltic_model.h"
#include "ccVector.h"

bc_DebugRenderContext *bc_createDebugRenderContext(htw_VkContext *vkContext, htw_BufferPool bufferPool, htw_DescriptorSetLayout perFrameLayout, htw_DescriptorSetLayout perPassLayout, u32 maxInstances) {

    htw_ShaderInputInfo colorInfo = {
        .size = sizeof(((bc_DebugInstanceData*)0)->color),
        .offset = offsetof(bc_DebugInstanceData, color),
        .inputType = HTW_VERTEX_TYPE_FLOAT
    };
    htw_ShaderInputInfo positionInfo = {
        .size = sizeof(((bc_DebugInstanceData*)0)->position),
        .offset = offsetof(bc_DebugInstanceData, position),
        .inputType = HTW_VERTEX_TYPE_FLOAT
    };
    htw_ShaderInputInfo sizeInfo = {
        .size = sizeof(((bc_DebugInstanceData*)0)->size),
        .offset = offsetof(bc_DebugInstanceData, size),
        .inputType = HTW_VERTEX_TYPE_FLOAT
    };
    htw_ShaderInputInfo instanceInfos[] = {colorInfo, positionInfo, sizeInfo};
    htw_ShaderSet shaderSet = {
        .vertexShader = htw_loadShader(vkContext, "shaders_bin/debug.vert.spv"),
        .fragmentShader = htw_loadShader(vkContext, "shaders_bin/debug.frag.spv"),
        .vertexInputCount = 0,
        .instanceInputStride = sizeof(bc_DebugInstanceData),
        .instanceInputCount = 3,
        .instanceInputInfos = instanceInfos
    };

    htw_DescriptorSetLayout debugObjectLayout = htw_createPerObjectSetLayout(vkContext);
    htw_DescriptorSetLayout debugPipelineLayouts[] = {perFrameLayout, perPassLayout, NULL, debugObjectLayout};
    htw_PipelineHandle pipeline = htw_createPipeline(vkContext, debugPipelineLayouts, shaderSet);

    size_t instanceDataSize = sizeof(bc_DebugInstanceData) * maxInstances;
    htw_MeshBufferSet model = {
        .vertexCount = 24,
        .instanceCount = maxInstances,
        .instanceBuffer = htw_createBuffer(vkContext, bufferPool, instanceDataSize, HTW_BUFFER_USAGE_VERTEX)
    };

    bc_Mesh instancedModel = {
        .meshBufferSet = model,
        .instanceData = calloc(1, instanceDataSize),
        .instanceDataSize = instanceDataSize
    };

    bc_DebugRenderContext *newRenderContext = calloc(1, sizeof(bc_DebugRenderContext));
    *newRenderContext = (bc_DebugRenderContext){
        .maxInstanceCount = maxInstances,
        .pipeline = pipeline,
        .debugLayout = debugObjectLayout,
        .debugDescriptorSet = htw_allocateDescriptor(vkContext, debugObjectLayout),
        .instancedModel = instancedModel
    };
    return newRenderContext;
}

bc_DebugInstanceData *bc_createDebugInstancePool(htw_VkContext *vkContext, size_t poolItemsCount) {
    bc_DebugInstanceData *instanceData = calloc(poolItemsCount, sizeof(bc_DebugInstanceData));
    return instanceData;
}

void bc_updateDebugModel(htw_VkContext *vkContext, bc_DebugRenderContext *rc) {
    htw_writeBuffer(vkContext, rc->instancedModel.meshBufferSet.instanceBuffer, rc->instancedModel.instanceData, rc->instancedModel.instanceDataSize);
}

void bc_drawDebugInstances(htw_VkContext *vkContext, bc_DebugRenderContext *rc) {
    static mat4x4 modelMatrix;

    mat4x4SetTranslation(modelMatrix, (vec3){{0.0, 0.0, 0.0}});
    htw_bindPipeline(vkContext, rc->pipeline);
    htw_bindDescriptorSet(vkContext, rc->pipeline, rc->debugDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_OBJECT);
    htw_drawPipelineX4(vkContext, rc->pipeline, &rc->instancedModel.meshBufferSet, HTW_DRAW_TYPE_INSTANCED, (float*)modelMatrix);
}
