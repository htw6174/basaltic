
#include "htw_core.h"
#include "htw_vulkan.h"
#include "ccVector.h"
#include "basaltic_window.h"
#include "basaltic_render.h"
#include "basaltic_logic.h"
#include "hexmapRender.h"
#include "htw_geomap.h"

typedef struct {
    mat4x4 pv;
    mat4x4 m;
} PushConstantData;

typedef struct private_bc_RenderContext {
    bc_RenderableHexmap *renderableHexmap;
    bc_HexmapTerrain *surfaceTerrain;
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
    //initTextGraphics(graphics);
    prc->renderableHexmap = bc_createRenderableHexmap(rc);
    prc->surfaceTerrain = bc_createHexmapTerrain(rc);
    //initDebugGraphics(graphics);

    htw_finalizeBufferPool(vkContext, rc->bufferPool);

    // write data from host into buffers (device memory)
    htw_beginOneTimeCommands(vkContext);
    //writeTextBuffers(graphics); // Must be within a oneTimeCommand execution to transition image layouts TODO: is there a better way to structure things, to make this more clear?
    htw_endOneTimeCommands(vkContext);

    bc_writeTerrainBuffers(rc, rc->internalRenderContext->renderableHexmap);

    // assign buffers to descriptor sets
    htw_updatePerFrameDescriptor(vkContext, rc->perFrameDescriptorSet, rc->windowInfoBuffer, rc->feedbackInfoBuffer, rc->worldInfoBuffer);

    //updateTextDescriptors(graphics);
    bc_updateHexmapDescriptors(rc, prc->renderableHexmap, prc->surfaceTerrain);

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
        bc_setRenderWorldWrap(rc, world->worldWidth, world->worldHeight);
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
        bc_updateTerrainVisibleChunks(rc, world, rc->internalRenderContext->surfaceTerrain, centerChunk);

        htw_pushConstants(rc->vkContext, rc->internalRenderContext->renderableHexmap->pipeline, &mvp);
        bc_drawHexmapTerrain(rc, world, rc->internalRenderContext->renderableHexmap, rc->internalRenderContext->surfaceTerrain);
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
    rc->worldInfo.totalWidth = world->worldWidth;
    // NOTE/TODO: does it make sense to split this info into different buffers for constants and per-tick values?
    htw_writeBuffer(rc->vkContext, rc->worldInfoBuffer, &rc->worldInfo, sizeof(bc_WorldInfo));
}

static void createDefaultPipeline(bc_RenderContext *rc) {
    htw_ShaderInputInfo positionInfo = {
        .size = sizeof(vec3),
        .offset = 0,
        .inputType = HTW_VERTEX_TYPE_FLOAT
    };
    htw_ShaderInputInfo vertexInfos[] = {positionInfo};
    htw_ShaderSet shaderSet = {
        .vertexShader = htw_loadShader(rc->vkContext, "shaders_bin/debug.vert.spv"),
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
