
#include <math.h>
#include <stdbool.h>
#include "htw_core.h"
#include "htw_random.h"
#include "basaltic_graphics.h"
#include "basaltic_render.h"
#include "basaltic_view.h"
#include "basaltic_model.h"
#include "basaltic_uiState.h"
#include "basaltic_defs.h"

typedef struct {
    vec3 position;
    float size;
} DebugInstanceData;

static void initDebugGraphics(bc_GraphicsState *graphics);
static void updateDebugGraphics(bc_GraphicsState *graphics, bc_WorldState *world);

static void initDebugGraphics(bc_GraphicsState *graphics) {
    graphics->showCharacterDebug = true;

    u32 maxInstanceCount = 1024; // TODO: needs to be set by world's total character count?
    htw_ShaderInputInfo positionInfo = {
        .size = sizeof(((DebugInstanceData*)0)->position),
        .offset = offsetof(DebugInstanceData, position),
        .inputType = HTW_VERTEX_TYPE_FLOAT
    };
    htw_ShaderInputInfo sizeInfo = {
        .size = sizeof(((DebugInstanceData*)0)->size),
        .offset = offsetof(DebugInstanceData, size),
        .inputType = HTW_VERTEX_TYPE_FLOAT
    };
    htw_ShaderInputInfo instanceInfos[] = {positionInfo, sizeInfo};
    htw_ShaderSet shaderSet = {
        .vertexShader = htw_loadShader(graphics->vkContext, "shaders_bin/debug.vert.spv"),
        .fragmentShader = htw_loadShader(graphics->vkContext, "shaders_bin/debug.frag.spv"),
        .vertexInputCount = 0,
        .instanceInputStride = sizeof(DebugInstanceData),
        .instanceInputCount = 2,
        .instanceInputInfos = instanceInfos
    };

    htw_DescriptorSetLayout debugObjectLayout = htw_createPerObjectSetLayout(graphics->vkContext);
    htw_DescriptorSetLayout debugPipelineLayouts[] = {graphics->perFrameLayout, graphics->perPassLayout, NULL, debugObjectLayout};
    htw_PipelineHandle pipeline = htw_createPipeline(graphics->vkContext, debugPipelineLayouts, shaderSet);

    size_t instanceDataSize = sizeof(DebugInstanceData) * maxInstanceCount;
    htw_ModelData model = {
        .vertexCount = 24,
        .instanceCount = maxInstanceCount,
        .instanceBuffer = htw_createBuffer(graphics->vkContext, graphics->bufferPool, instanceDataSize, HTW_BUFFER_USAGE_VERTEX)
    };

    bc_Model instancedModel = {
        .model = model,
        .instanceData = malloc(instanceDataSize),
        .instanceDataSize = instanceDataSize
    };

    graphics->characterDebug = (bc_DebugSwarm){
        .maxInstanceCount = maxInstanceCount,
        .pipeline = pipeline,
        .debugLayout = debugObjectLayout,
        .debugDescriptorSet = htw_allocateDescriptor(graphics->vkContext, debugObjectLayout),
        .instancedModel = instancedModel
    };
}

static void updateDebugGraphics(bc_GraphicsState *graphics, bc_WorldState *world) {
    bc_Model *characterDebugModel = &graphics->characterDebug.instancedModel;
    DebugInstanceData *instanceData = characterDebugModel->instanceData;
    for (int i = 0; i < characterDebugModel->model.instanceCount; i++) {
        bc_Character *character = &world->characters[i];
        htw_geo_GridCoord characterCoord = character->currentState.worldCoord;
        u32 chunkIndex, cellIndex;
        bc_gridCoordinatesToChunkAndCell(world, characterCoord, &chunkIndex, &cellIndex);
        s32 elevation = htw_geo_getMapValueByIndex(world->chunks[chunkIndex].heightMap, cellIndex);
        float posX, posY;
        htw_geo_getHexCellPositionSkewed(characterCoord, &posX, &posY);
        DebugInstanceData characterData = {
            .position = (vec3){{posX, posY, elevation}},
            .size = 1
        };
        instanceData[i] = characterData;
    }
    htw_writeBuffer(graphics->vkContext, characterDebugModel->model.instanceBuffer, characterDebugModel->instanceData, characterDebugModel->instanceDataSize);
}

int bc_drawFrame(bc_GraphicsState *graphics, bc_UiState *ui, bc_WorldState *world) {

    {

        updateDebugGraphics(graphics, world);
    }

    if (ui->interfaceMode == BC_INTERFACE_MODE_GAMEPLAY) {
        if (world != NULL) {

            // update ui with feedback info from shaders
            htw_retreiveBuffer(graphics->vkContext, graphics->feedbackInfoBuffer, &graphics->feedbackInfo, sizeof(_bc_FeedbackInfo));
            ui->hoveredChunkIndex = graphics->feedbackInfo.chunkIndex;
            ui->hoveredCellIndex = graphics->feedbackInfo.cellIndex;
        }

        if (graphics->showCharacterDebug) {
            // draw character debug shapes
            mat4x4SetTranslation(mvp.m, (vec3){{0.0, 0.0, 0.0}});
            htw_pushConstants(graphics->vkContext, graphics->characterDebug.pipeline, &mvp);
            htw_bindPipeline(graphics->vkContext, graphics->characterDebug.pipeline);
            htw_bindDescriptorSet(graphics->vkContext, graphics->characterDebug.pipeline, graphics->characterDebug.debugDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_OBJECT);
            htw_drawPipeline(graphics->vkContext, graphics->characterDebug.pipeline, &graphics->characterDebug.instancedModel.model, HTW_DRAW_TYPE_INSTANCED);
        }
    }

    if (ui->interfaceMode == BC_INTERFACE_MODE_SYSTEM_MENU) {
        // draw text overlays
        drawDebugText(graphics, ui);
    }

    return 0;
}

