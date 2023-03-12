
#include "htw_core.h"
#include "htw_vulkan.h"
#include "ccVector.h"
#include "basaltic_window.h"
#include "basaltic_render.h"
#include "basaltic_logic.h"
#include "hexmapRender.h"
#include "debugRender.h"
#include "htw_geomap.h"

typedef struct {
    mat4x4 pv;
    mat4x4 m;
} PushConstantData;

// TODO: roll this into the parent struct, now that I don't have to worry about circular includes?
typedef struct private_bc_RenderContext {
    bc_RenderableHexmap *renderableHexmap;
    bc_HexmapTerrain *surfaceTerrain;
    bc_DebugRenderContext *debugContext;
} private_bc_RenderContext;

static void updateWindowInfoBuffer(bc_RenderContext *rc, bc_WindowContext *wc, s32 mouseX, s32 mouseY, vec4 cameraPosition, vec4 cameraFocalPoint);
static void updateWorldInfoBuffer(bc_RenderContext *rc, bc_WorldState *world);

static void createDefaultPipeline(bc_RenderContext *rc);

bc_RenderContext* bc_createRenderContext(bc_WindowContext* wc) {
    bc_RenderContext *rc = calloc(1, sizeof(bc_RenderContext));
    private_bc_RenderContext *prc = calloc(1, sizeof(private_bc_RenderContext));
    rc->internalRenderContext = prc;

    htw_VkContext *vkContext = wc->vkContext;
    rc->vkContext = vkContext;

    rc->bufferPool = htw_createBufferPool(vkContext, 100, HTW_BUFFER_POOL_TYPE_DIRECT);

    // create per frame descriptor buffers
    rc->windowInfoBuffer = htw_createBuffer(vkContext, rc->bufferPool, sizeof(bc_WindowInfo), HTW_BUFFER_USAGE_UNIFORM);
    rc->feedbackInfoBuffer = htw_createBuffer(vkContext, rc->bufferPool, sizeof(bc_FeedbackInfo), HTW_BUFFER_USAGE_STORAGE);
    rc->worldInfoBuffer = htw_createBuffer(vkContext, rc->bufferPool, sizeof(bc_WorldInfo), HTW_BUFFER_USAGE_UNIFORM);

    // setup descriptor sets
    rc->perFrameLayout = htw_createPerFrameSetLayout(vkContext);
    rc->perFrameDescriptorSet = htw_allocateDescriptor(vkContext, rc->perFrameLayout);
    rc->perPassLayout = htw_createPerPassSetLayout(vkContext);
    rc->perPassDescriptorSet = htw_allocateDescriptor(vkContext, rc->perPassLayout);

    // setup model data, shaders, pipelines, buffers, and descriptor sets for rendered objects
    createDefaultPipeline(rc);

    // allocate device buffers to be written to later
    {
        //initTextGraphics(graphics);
        prc->renderableHexmap = bc_createRenderableHexmap(rc->vkContext, rc->bufferPool, rc->perFrameLayout, rc->perPassLayout);
        prc->surfaceTerrain = bc_createHexmapTerrain(rc->vkContext, rc->bufferPool);
        prc->debugContext = bc_createDebugRenderContext(rc->vkContext, rc->bufferPool, rc->perFrameLayout, rc->perPassLayout);
        //initDebugGraphics(graphics);
    }
    htw_finalizeBufferPool(vkContext, rc->bufferPool);

    // write data from host into device memory buffers
    {
        // do any buffer updates that require a vkCommandBuffer e.g. image layout transitions
        {
            htw_beginOneTimeCommands(vkContext);
            //writeTextBuffers(graphics);
            htw_endOneTimeCommands(vkContext);
        }

        bc_writeTerrainBuffers(rc->vkContext, rc->internalRenderContext->renderableHexmap);
    }

    // assign buffers to descriptor sets
    {
        htw_updatePerFrameDescriptor(vkContext, rc->perFrameDescriptorSet, rc->windowInfoBuffer, rc->feedbackInfoBuffer, rc->worldInfoBuffer);

        //updateTextDescriptors(graphics);
        bc_updateHexmapDescriptors(rc->vkContext, prc->renderableHexmap, prc->surfaceTerrain);
    }

    // initialize highlighted cell
    rc->feedbackInfo = (bc_FeedbackInfo){
        .cellIndex = 0,
        .chunkIndex = 0,
    };

    // initialize world info
    rc->worldInfo = (bc_WorldInfo){
        .gridToWorld = (vec3){{1.0, 1.0, 0.2}},
        .timeOfDay = 0,
        .totalWidth = 0,
        .seaLevel = 0,
        .visibilityOverrideBits = BC_TERRAIN_VISIBILITY_ALL,
    };

    return rc;
}

void bc_updateRenderContextWithWorldParams(bc_RenderContext *rc, bc_WorldState *world) {
    if (world != NULL) {
        bc_setRenderWorldWrap(rc, world->surfaceMap->mapWidth, world->surfaceMap->mapHeight);
        updateWorldInfoBuffer(rc, world);
    }
}

void bc_updateRenderContextWithUiState(bc_RenderContext *rc, bc_WindowContext *wc, bc_UiState *ui) {

    // translate camera parameters to uniform buffer info + projection and view matricies TODO: camera position interpolation, from current to target
    vec4 cameraPosition;
    vec4 cameraFocalPoint;
    {
        float pitch = ui->cameraPitch * DEG_TO_RAD;
        float yaw = ui->cameraYaw * DEG_TO_RAD;
        float correctedDistance = powf(ui->cameraDistance, 2.0);
        float radius = cos(pitch) * correctedDistance;
        float height = sin(pitch) * correctedDistance;
        float floor = ui->cameraElevation; //ui->activeLayer == BC_WORLD_LAYER_SURFACE ? 0 : -8;
        cameraPosition = (vec4){
            .x = ui->cameraX + radius * sin(yaw),
            .y = ui->cameraY + radius * -cos(yaw),
            .z = floor + height
        };
        cameraFocalPoint = (vec4){
            .x = ui->cameraX,
            .y = ui->cameraY,
            .z = floor
        };
        updateWindowInfoBuffer(rc, wc, ui->mouse.x, ui->mouse.y, cameraPosition, cameraFocalPoint);

        if (ui->cameraMode == BC_CAMERA_MODE_ORTHOGRAPHIC) {
            mat4x4Orthographic(rc->camera.projection, ((float)wc->width / wc->height) * correctedDistance, correctedDistance, 0.1f, 10000.f);
        } else {
            mat4x4Perspective(rc->camera.projection, PI / 3.f, (float)wc->width / wc->height, 0.1f, 10000.f);
        }
        mat4x4LookAt(rc->camera.view,
                    cameraPosition.xyz,
                    cameraFocalPoint.xyz,
                    (vec3){ {0.f, 0.f, 1.f} });
    }

    // update ui with feedback info from shaders
    htw_retreiveBuffer(rc->vkContext, rc->feedbackInfoBuffer, &rc->feedbackInfo, sizeof(bc_FeedbackInfo));
    ui->hoveredChunkIndex = rc->feedbackInfo.chunkIndex;
    ui->hoveredCellIndex = rc->feedbackInfo.cellIndex;
}

void bc_renderFrame(bc_RenderContext *rc, bc_WorldState *world) {

    PushConstantData mvp;
    mat4x4MultiplyMatrix(mvp.pv, rc->camera.view, rc->camera.projection);
    mat4x4Identity(mvp.m); // remember to reinitialize those matricies every frame

    htw_bindDescriptorSet(rc->vkContext, rc->defaultPipeline, rc->perFrameDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_FRAME);
    htw_bindDescriptorSet(rc->vkContext, rc->defaultPipeline, rc->perPassDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_PASS);

    if (world != NULL) {
        // determine the indicies of chunks closest to the camera
        float cameraX = rc->windowInfo.cameraFocalPoint.x;
        float cameraY = rc->windowInfo.cameraFocalPoint.y;
        u32 centerChunk = bc_getChunkIndexByWorldPosition(world, cameraX, cameraY);
        bc_updateTerrainVisibleChunks(rc->vkContext, world->surfaceMap, rc->internalRenderContext->surfaceTerrain, centerChunk);

        // TODO: find a home for this logic. Maybe a character-specific renderer class.
        bc_Mesh *characterDebugModel = &rc->internalRenderContext->debugContext->instancedModel;
        bc_DebugInstanceData *instanceData = characterDebugModel->instanceData;
        for (int i = 0; i < characterDebugModel->meshBufferSet.instanceCount; i++) {
            bc_Character *character = &world->characters[i];
            htw_geo_GridCoord characterCoord = character->currentState.worldCoord;
            u32 chunkIndex, cellIndex;
            htw_geo_gridCoordinateToChunkAndCellIndex(world->surfaceMap, characterCoord, &chunkIndex, &cellIndex);
            s32 elevation = bc_getCellByIndex(world->surfaceMap, chunkIndex, cellIndex)->height;
            float posX, posY;
            htw_geo_getHexCellPositionSkewed(characterCoord, &posX, &posY);
            bc_DebugInstanceData characterData = {
                .color = (vec4){{0.0, 0.0, 1.0, 0.5}},
                .position = (vec3){{posX, posY, elevation}},
                .size = 1.0
            };
            instanceData[i] = characterData;
        }
        bc_updateDebugModel(rc->vkContext, rc->internalRenderContext->debugContext);

        // NOTE/TODO: need to figure out a best practice regarding push constant updates. Would be reasonable to assume that in this engine, push constants are always used for mvp matricies in [pv, m] layout (at least for world space objects?). Push constants persist through different rendering pipelines, so I could push the pv matrix once per frame and not have to pass camera matrix info to any of the sub-rendering methods like drawHexmapTerrain. Those methods would only have to be responsible for updating the model portion of the push constant range. QUESTION: is this a good way to do things, or am I setting up a trap by not assigning the pv matrix per-pipeline?
        htw_pushConstants(rc->vkContext, rc->defaultPipeline, &mvp);
        bc_drawHexmapTerrain(rc->vkContext, world->surfaceMap, rc->internalRenderContext->renderableHexmap, rc->internalRenderContext->surfaceTerrain, rc->wrapInstancePositions);
        bc_drawDebugInstances(rc->vkContext, rc->internalRenderContext->debugContext);
    }
}

void bc_setRenderWorldWrap(bc_RenderContext *rc, u32 worldWidth, u32 worldHeight) {
    float x00, y00;
    htw_geo_getHexCellPositionSkewed((htw_geo_GridCoord){-worldWidth, -worldHeight}, &x00, &y00);
    float x10 = htw_geo_getHexPositionX(0, -worldHeight);
    float x01 = htw_geo_getHexPositionX(-worldWidth, 0);
    float x11 = 0.0;
    float y10 = y00;
    float y01 = 0.0;
    float y11 = 0.0;
    rc->wrapInstancePositions[0] = (vec3){{x00, y00, 0.0}};
    rc->wrapInstancePositions[1] = (vec3){{x01, y01, 0.0}};
    rc->wrapInstancePositions[2] = (vec3){{x10, y10, 0.0}};
    rc->wrapInstancePositions[3] = (vec3){{x11, y11, 0.0}};
}

static void updateWindowInfoBuffer(bc_RenderContext *rc, bc_WindowContext *wc, s32 mouseX, s32 mouseY, vec4 cameraPosition, vec4 cameraFocalPoint) {
    rc->windowInfo = (bc_WindowInfo){
        .windowSize = (vec2){{wc->width, wc->height}},
        .mousePosition = (vec2){{mouseX, mouseY}},
        .cameraPosition = cameraPosition,
        .cameraFocalPoint = cameraFocalPoint
    };
    htw_writeBuffer(rc->vkContext, rc->windowInfoBuffer, &rc->windowInfo, sizeof(bc_WindowInfo));
}

static void updateWorldInfoBuffer(bc_RenderContext *rc, bc_WorldState *world) {
    // TODO: update parts of this per simulation tick
    rc->worldInfo.totalWidth = world->surfaceMap->mapWidth;
    // NOTE/TODO: does it make sense to split this info into different buffers for constants and per-tick values?
    htw_writeBuffer(rc->vkContext, rc->worldInfoBuffer, &rc->worldInfo, sizeof(bc_WorldInfo));
}

// NOTE/TODO: is it better to add more arguments to this kind of method, to make it clear what needs to be setup first? e.g. this needs the perFrame and perPass layouts created first
static void createDefaultPipeline(bc_RenderContext *rc) {
    htw_ShaderInputInfo positionInfo = {
        .size = sizeof(vec3),
        .offset = 0,
        .inputType = HTW_VERTEX_TYPE_FLOAT
    };
    htw_ShaderInputInfo vertexInfos[] = {positionInfo};
    htw_ShaderSet shaderSet = {
        .vertexShader = htw_loadShader(rc->vkContext, "shaders_bin/debug.vert.spv"), // TODO: default shader for simple meshes
        .fragmentShader = htw_loadShader(rc->vkContext, "shaders_bin/debug.frag.spv"),
        .vertexInputStride = sizeof(vec3),
        .vertexInputCount = 1,
        .vertexInputInfos = vertexInfos,
        .instanceInputCount = 0,
    };

    htw_DescriptorSetLayout defaultObjectLayout = htw_createPerObjectSetLayout(rc->vkContext);
    htw_DescriptorSetLayout defaultPipelineLayouts[] = {rc->perFrameLayout, rc->perPassLayout, NULL, defaultObjectLayout};
    rc->defaultPipeline = htw_createPipeline(rc->vkContext, defaultPipelineLayouts, shaderSet);
}
