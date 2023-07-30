#include <SDL2/SDL_mutex.h>
#include "htw_core.h"
#include "ccVector.h"
#include "basaltic_window.h"
#include "basaltic_render.h"
#include "basaltic_uniforms.h"
#include "basaltic_logic.h"
#include "components/basaltic_components_planes.h"
#include "hexmapRender.h"
#include "debugRender.h"
#include "htw_geomap.h"
#include "sokol_gfx.h"
#include "flecs.h"

// TODO: roll this into the parent struct, now that I don't have to worry about circular includes?
typedef struct private_bc_RenderContext {
    bc_RenderableHexmap *renderableHexmap;
    bc_HexmapTerrain *surfaceTerrain;
    bc_DebugRenderContext *debugContext;

    // TEST: triangle data
    sg_pipeline testPipeline;
    sg_bindings testBinding;
} private_bc_RenderContext;

static void updateWindowInfoBuffer(bc_RenderContext *rc, bc_WindowContext *wc, s32 mouseX, s32 mouseY, vec4 cameraPosition, vec4 cameraFocalPoint);
static void updateWorldInfoBuffer(bc_RenderContext *rc, bc_WorldState *world);

static void drawCharacterDebug(bc_RenderContext *rc, bc_WorldState *world);

static void createDefaultPipeline(bc_RenderContext *rc);

bc_RenderContext* bc_createRenderContext(bc_WindowContext* wc) {
    bc_RenderContext *rc = calloc(1, sizeof(bc_RenderContext));
    private_bc_RenderContext *prc = calloc(1, sizeof(private_bc_RenderContext));
    rc->internalRenderContext = prc;

    // DEFAULTS TODO: make these components
    vc.rc = (bc_RenderContext){
        .windowInfo.visibilityRadius = 512,
        .worldInfo.gridToWorld = (vec3){{1.0, 1.0, 0.2}},
        .worldInfo.visibilityOverrideBits = BC_TERRAIN_VISIBILITY_ALL,
        .chunkVisibilityRadius = 3
    };

    // create per frame descriptor buffers
    // rc->windowInfoBuffer = htw_createBuffer(vkContext, rc->bufferPool, sizeof(bc_WindowInfo), HTW_BUFFER_USAGE_UNIFORM);
    // rc->feedbackInfoBuffer = htw_createBuffer(vkContext, rc->bufferPool, sizeof(bc_FeedbackInfo), HTW_BUFFER_USAGE_STORAGE);
    // rc->worldInfoBuffer = htw_createBuffer(vkContext, rc->bufferPool, sizeof(bc_WorldInfo), HTW_BUFFER_USAGE_UNIFORM);
    sg_shader_uniform_block_desc windowBufferDesc = {
        .size = sizeof(bc_WindowInfo),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {}
    };
    rc->windowInfoBuffer = windowBufferDesc;

    // setup descriptor sets
    // rc->perFrameLayout = htw_createPerFrameSetLayout(vkContext);
    // rc->perFrameDescriptorSet = htw_allocateDescriptor(vkContext, rc->perFrameLayout);
    // rc->perPassLayout = htw_createPerPassSetLayout(vkContext);
    // rc->perPassDescriptorSet = htw_allocateDescriptor(vkContext, rc->perPassLayout);

    // setup model data, shaders, pipelines, buffers, and descriptor sets for rendered objects
    //createDefaultPipeline(rc);

    // allocate device buffers to be written to later
    {
        //initTextGraphics(graphics);
        //prc->renderableHexmap = bc_createRenderableHexmap(rc->perFrameUniformsVert, rc->perFrameUniformsFrag);
        //prc->surfaceTerrain = bc_createHexmapTerrain(prc->renderableHexmap, 2);
        //prc->debugContext = bc_createDebugRenderContext(rc->vkContext, rc->bufferPool, rc->perFrameLayout, rc->perPassLayout, BC_MAX_CHARACTERS);
        //initDebugGraphics(graphics);
    }

    //bc_writeTerrainBuffers(rc->internalRenderContext->renderableHexmap);

    // assign buffers to descriptor sets
    {
        //htw_updatePerFrameDescriptor(vkContext, rc->perFrameDescriptorSet, rc->windowInfoBuffer, rc->feedbackInfoBuffer, rc->worldInfoBuffer);

        //updateTextDescriptors(graphics);
        //bc_updateHexmapDescriptors(prc->surfaceTerrain);
    }

    // window info defaults
    rc->windowInfo = (bc_WindowInfo){
        .visibilityRadius = 3 * 32,
        .fogExtinction = 0.003,
        .fogInscattering = 0.0015
    };

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

    rc->chunkVisibilityRadius = 3;

    rc->drawSystems = false;

    return rc;
}

void bc_setWorldRenderScale(bc_RenderContext *rc, vec3 worldScale) {
    rc->worldInfo.gridToWorld = worldScale;
    //bc_setRenderWorldWrap(rc, 0, 0);
}

void bc_updateRenderContextWithWorldParams(bc_RenderContext *rc, bc_WorldState *world) {
    if (world != NULL) {
        htw_ChunkMap *cm = ecs_get(world->ecsWorld, world->centralPlane, Plane)->chunkMap;
        bc_setRenderWorldWrap(rc, cm->mapWidth, cm->mapHeight);
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
    //htw_retreiveBuffer(rc->vkContext, rc->feedbackInfoBuffer, &rc->feedbackInfo, sizeof(bc_FeedbackInfo));
    ui->hoveredChunkIndex = rc->feedbackInfo.chunkIndex;
    ui->hoveredCellIndex = rc->feedbackInfo.cellIndex;
}

void bc_renderFrame(bc_RenderContext *rc, bc_WorldState *world) {

    bc_mvp mvp;
    mat4x4MultiplyMatrix(mvp.pv, rc->camera.view, rc->camera.projection);
    mat4x4Identity(mvp.m); // remember to reinitialize those matricies every frame

    // TEST: draw triangle
    {
        sg_apply_pipeline(rc->internalRenderContext->testPipeline);
        sg_apply_bindings(&rc->internalRenderContext->testBinding);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(mvp));
        sg_draw(0, 3, 1);
    }

    // TEST: draw single chunk
    {
        // sg_apply_pipeline(rc->internalRenderContext->renderableHexmap->pipeline);
        // sg_apply_bindings(&rc->internalRenderContext->renderableHexmap->bindings);
        // sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(mvp));
        // sg_draw(0, rc->internalRenderContext->renderableHexmap->indexCount, 1);
    }

    //htw_bindDescriptorSet(rc->vkContext, rc->defaultPipeline, rc->perFrameDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_FRAME);
    //htw_bindDescriptorSet(rc->vkContext, rc->defaultPipeline, rc->perPassDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_PASS);

    if (world != NULL) {
        updateWorldInfoBuffer(rc, world);
        //htw_pushConstants(rc->vkContext, rc->defaultPipeline, &mvp);

        // determine the indicies of chunks closest to the camera
        htw_ChunkMap *cm = ecs_get(world->ecsWorld, world->centralPlane, Plane)->chunkMap;
        float cameraX = rc->windowInfo.cameraFocalPoint.x;
        float cameraY = rc->windowInfo.cameraFocalPoint.y;
        u32 centerChunk = bc_getChunkIndexByWorldPosition(cm, cameraX, cameraY);
        bc_updateTerrainVisibleChunks(cm, rc->internalRenderContext->surfaceTerrain, centerChunk);

        // NOTE/TODO: need to figure out a best practice regarding push constant updates. Would be reasonable to assume that in this engine, push constants are always used for mvp matricies in [pv, m] layout (at least for world space objects?). Push constants persist through different rendering pipelines, so I could push the pv matrix once per frame and not have to pass camera matrix info to any of the sub-rendering methods like drawHexmapTerrain. Those methods would only have to be responsible for updating the model portion of the push constant range. QUESTION: is this a good way to do things, or am I setting up a trap by not assigning the pv matrix per-pipeline?
        //bc_drawHexmapTerrain(cm, rc->internalRenderContext->renderableHexmap, rc->internalRenderContext->surfaceTerrain, mvp, rc->windowInfo.mousePosition, &rc->feedbackInfo);

        if (rc->drawSystems) {
            // Wait with a timeout to prevent framerate from dropping or locking entirely
            // FIXME: if model thread tick rate is too high, it monopolizes the semaphore and doesn't allow the view thread to access ecs data. Any way to have the model thread 'give up control' every once in a while, without manually pausing it?
            if (SDL_SemWaitTimeout(world->lock, 16) != SDL_MUTEX_TIMEDOUT) {
                // Only safe to iterate queries while the world is in readonly mode, or has exclusive access from one thread
                // TODO: inspector reflection for systems, option to disable them individually, turn draw functions into systems
                drawCharacterDebug(rc, world);
                SDL_SemPost(world->lock);
            }
            //bc_drawDebugInstances(rc->vkContext, rc->internalRenderContext->debugContext);
        }
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
    rc->wrapInstancePositions[0] = vec3MultiplyVector((vec3){{x00, y00, 0.0}}, rc->worldInfo.gridToWorld);
    rc->wrapInstancePositions[1] = vec3MultiplyVector((vec3){{x01, y01, 0.0}}, rc->worldInfo.gridToWorld);
    rc->wrapInstancePositions[2] = vec3MultiplyVector((vec3){{x10, y10, 0.0}}, rc->worldInfo.gridToWorld);
    rc->wrapInstancePositions[3] = vec3MultiplyVector((vec3){{x11, y11, 0.0}}, rc->worldInfo.gridToWorld);
    //htw_setModelTranslationInstances(rc->vkContext, (float*)rc->wrapInstancePositions);
}

static void updateWindowInfoBuffer(bc_RenderContext *rc, bc_WindowContext *wc, s32 mouseX, s32 mouseY, vec4 cameraPosition, vec4 cameraFocalPoint) {
    bc_WindowInfo *windowInfo = &rc->windowInfo;
    windowInfo->windowSize = (vec2){{wc->width, wc->height}};
    windowInfo->mousePosition = (vec2){{mouseX, mouseY}};
    windowInfo->cameraPosition = cameraPosition;
    windowInfo->cameraFocalPoint = cameraFocalPoint;
    //htw_writeBuffer(rc->vkContext, rc->windowInfoBuffer, &rc->windowInfo, sizeof(bc_WindowInfo));
}

static void updateWorldInfoBuffer(bc_RenderContext *rc, bc_WorldState *world) {
    // TODO: update parts of this per simulation tick
    //rc->worldInfo.totalWidth = world->surfaceMap->mapWidth;
    // NOTE/TODO: does it make sense to split this info into different buffers for constants and per-tick values?
    rc->worldInfo.timeOfDay = world->step % 24;
    //htw_writeBuffer(rc->vkContext, rc->worldInfoBuffer, &rc->worldInfo, sizeof(bc_WorldInfo));
}

static void drawCharacterDebug(bc_RenderContext *rc, bc_WorldState *world) {
    // TODO: find a home for this logic. Maybe a character-specific renderer class.
    //bc_Mesh *characterDebugModel = &rc->internalRenderContext->debugContext->instancedModel;
    bc_DebugInstanceData *instanceData = rc->internalRenderContext->debugContext->instanceData;

    // TEST: should get terrainMap from entity relationship instead
    htw_ChunkMap *cm = ecs_get(world->ecsWorld, world->centralPlane, Plane)->chunkMap;

    ecs_iter_t it = ecs_query_iter(world->ecsWorld, world->characters);
    u32 i = 0; // To track mesh instance index
    // Outer loop: traverses different tables that match the query
    while (ecs_query_next(&it)) {
        Position *positions = ecs_field(&it, Position, 1);
        if (i >= rc->internalRenderContext->debugContext->maxInstanceCount) {
            ecs_iter_fini(&it);
            break;
        }
        // Inner loop: entities within a table
        for (int e = 0; e < it.count && i < rc->internalRenderContext->debugContext->maxInstanceCount; e++, i++) {
            htw_geo_GridCoord characterCoord = positions[e];
            u32 chunkIndex, cellIndex;
            htw_geo_gridCoordinateToChunkAndCellIndex(cm, characterCoord, &chunkIndex, &cellIndex);
            s32 elevation = bc_getCellByIndex(cm, chunkIndex, cellIndex)->height;
            float posX, posY;
            htw_geo_getHexCellPositionSkewed(characterCoord, &posX, &posY);

            vec4 color = {{0.0, 0.0, 1.0, 0.5}}; // default blue
            bc_DebugInstanceData characterData = {
                .color = color,
                .position = (vec3){{posX, posY, elevation}},
                .size = 1.0
            };
            instanceData[i] = characterData;
        }
    }
    //bc_updateDebugModel(rc->vkContext, rc->internalRenderContext->debugContext);
    // Cap rendered instances to current character count
    //characterDebugModel->meshBufferSet.instanceCount = i;

    // for (int i = 0; i < characterDebugModel->meshBufferSet.instanceCount; i++) {
    //     bc_Character *character = &world->characterPool->characters[i];
    //     htw_geo_GridCoord characterCoord = character->currentState.worldCoord;
    // }
}

// NOTE/TODO: is it better to add more arguments to this kind of method, to make it clear what needs to be setup first? e.g. this needs the perFrame and perPass layouts created first
// static void createDefaultPipeline(bc_RenderContext *rc) {
//     htw_ShaderInputInfo positionInfo = {
//         .size = sizeof(vec3),
//         .offset = 0,
//         .inputType = HTW_VERTEX_TYPE_FLOAT
//     };
//     htw_ShaderInputInfo vertexInfos[] = {positionInfo};
//     htw_ShaderSet shaderSet = {
//         .vertexShader = htw_loadShader(rc->vkContext, "shaders_bin/debug.vert.spv"), // TODO: default shader for simple meshes
//         .fragmentShader = htw_loadShader(rc->vkContext, "shaders_bin/debug.frag.spv"),
//         .vertexInputStride = sizeof(vec3),
//         .vertexInputCount = 1,
//         .vertexInputInfos = vertexInfos,
//         .instanceInputCount = 0,
//     };
//
//     htw_DescriptorSetLayout defaultObjectLayout = htw_createPerObjectSetLayout(rc->vkContext);
//     htw_DescriptorSetLayout defaultPipelineLayouts[] = {rc->perFrameLayout, rc->perPassLayout, NULL, defaultObjectLayout};
//     rc->defaultPipeline = htw_createPipeline(rc->vkContext, defaultPipelineLayouts, shaderSet);
// }