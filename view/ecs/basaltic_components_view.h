#ifndef BASALTIC_COMPONENTS_VIEW_H_INCLUDED
#define BASALTIC_COMPONENTS_VIEW_H_INCLUDED

#include "htw_core.h"
#include "basaltic_worldState.h"
#include "basaltic_components.h"
#include "ccVector.h"
#include "sokol_gfx.h"
#include "flecs.h"

#undef ECS_META_IMPL
#ifndef BASALTIC_VIEW_IMPL
#define ECS_META_IMPL EXTERN // Ensure meta symbols are only defined once
#endif

/* Common types */
extern ECS_COMPONENT_DECLARE(s32);
extern ECS_COMPONENT_DECLARE(vec3);

typedef vec3 Scale;
extern ECS_COMPONENT_DECLARE(Scale);



/* Connection to Model */

ECS_STRUCT(ModelWorld, {
    ecs_world_t *world;
});

ECS_STRUCT(ModelQuery, {
    ecs_query_t *query;
});

ECS_STRUCT(QueryDesc, {
    ecs_query_desc_t desc;
});

typedef u64 ModelLastRenderedStep;
extern ECS_COMPONENT_DECLARE(ModelLastRenderedStep);



/* User Interface */

ECS_STRUCT(Pointer, {
    s32 x;
    s32 y;
    s32 lastX;
    s32 lastY;
});

// TODO: setup camera 'real' position and 'target' position, smoothly interpolate each frame
ECS_STRUCT(Camera, {
    vec3 origin; // TODO: figure out how reflection info for this works? seems to run without complaints, even though there isn't an 'inner' reflection info defined
    float distance;
    float pitch;
    float yaw;
});

// max camera distance from origin, in hex coordinate space
ECS_STRUCT(CameraWrap, {
    float x;
    float y;
});

ECS_STRUCT(CameraSpeed, {
    float movement;
    float rotation;
});

ECS_STRUCT(RenderDistance, {
    u32 radius;
});

ECS_STRUCT(FocusPlane, {
    ecs_entity_t entity;
});

ECS_STRUCT(HoveredCell, {
    u32 chunkIndex;
    u32 cellIndex;
});

ECS_STRUCT(SelectedCell, {
    u32 chunkIndex;
    u32 cellIndex;
});



/* Rendering */

ECS_STRUCT(WindowSize, {
    float x;
    float y;
});

ECS_STRUCT(Mouse, {
    float x;
    float y;
});

ECS_STRUCT(PVMatrix, {
    mat4x4 pv;
});

ECS_STRUCT(ModelMatrix, {
    mat4x4 model;
});

ECS_STRUCT(Pipeline, {
    sg_pipeline pipeline;
});

// Relationship where the target is a Pipeline entity
extern ECS_TAG_DECLARE(RenderPipeline);

extern ECS_TAG_DECLARE(TerrainRender);
extern ECS_TAG_DECLARE(DebugRender);

ECS_STRUCT(WrapInstanceOffsets, {
    vec3 offsets[4];
});

ECS_STRUCT(InstanceBuffer, {
    sg_buffer buffer;
    s32 instances;
    size_t size;
    void *data;
});

/** Immutable buffers */
ECS_STRUCT(Mesh, {
    sg_buffer vertexBuffers[SG_MAX_SHADERSTAGE_BUFFERS - 1]; // 0th vertex buffer reserved for instance buffer
    sg_buffer indexBuffer;
    s32 elements;
});

/** Standalone element count, used for shaders that generate their own geometry */
ECS_STRUCT(Elements, {
    s32 count;
});

// TODO: textures

ECS_STRUCT(FeedbackBuffer, {
    u32 gluint;
});

ECS_STRUCT(TerrainBuffer, {
    u32 gluint;
    // Radius of visible chunks around the camera center. 1 = only chunk containing camera is visible; 2 = 3x3 area; 3 = 5x5 area; etc.
    u32 renderedChunkRadius;
    u32 renderedChunkCount;
    // length renderedChunkCount arrays that are compared to determine what chunks to reload each frame, and where to place them
    s32 *closestChunks;
    s32 *loadedChunks;
    vec3 *chunkPositions;
    // Size of the data buffered for each draw call; length of data array is chunkBufferCellCount * renderedChunkCount
    // NOTE: this is greater than the number of cells in a chunk, also includes adjacent chunk data
    size_t chunkBufferCellCount;
    bc_CellData *data;
});



void BasalticComponentsViewImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_VIEW_H_INCLUDED