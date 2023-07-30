#ifndef BASALTIC_COMPONENTS_VIEW_H_INCLUDED
#define BASALTIC_COMPONENTS_VIEW_H_INCLUDED

#include "htw_core.h"
#include "basaltic_worldState.h"
#include "basaltic_components.h"
#include <time.h>
#include "ccVector.h"
#include "sokol_gfx.h"
#include "flecs.h"

#undef ECS_META_IMPL
#ifndef BASALTIC_VIEW_IMPL
#define ECS_META_IMPL EXTERN // Ensure meta symbols are only defined once
#endif

#define MAX_QUERY_EXPR_LENGTH 1024

/* Common types */
extern ECS_COMPONENT_DECLARE(s32);
extern ECS_COMPONENT_DECLARE(vec3);

typedef vec3 Scale;
extern ECS_COMPONENT_DECLARE(Scale);

typedef vec4 Color;
extern ECS_COMPONENT_DECLARE(Color);

ECS_STRUCT(ResourceFile, {
    // NOTE: should be explicitly time_t; need to add meta info first
    u64 accessTime; // update whenever file is read; if modify time on disk is > this, should reload file
    char path[256];
});



/* Connection to Model */

ECS_STRUCT(ModelWorld, {
    ecs_world_t *world;
});

ECS_STRUCT(ModelQuery, {
    ecs_query_t *query;
});

ECS_STRUCT(QueryDesc, {
    ecs_query_desc_t desc;
    char expr[MAX_QUERY_EXPR_LENGTH]; // query description doesn't reserve space for the filter expression, store here TODO: change do a resizable type
});

typedef u64 ModelLastRenderedStep;
extern ECS_COMPONENT_DECLARE(ModelLastRenderedStep);



/* User Interface */

extern ECS_TAG_DECLARE(Previous); // Relationship where target is previous value of a component

ECS_STRUCT(DeltaTime, {
    float seconds;
});

// Pixel coordinates directly from SDL; origin at top-left
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

// TODO: consider adding a flag to this, reset every frame and only set in shader feedback if any cell is hovered
ECS_STRUCT(HoveredCell, {
    s32 x;
    s32 y;
});

ECS_STRUCT(SelectedCell, {
    s32 x;
    s32 y;
});

ECS_STRUCT(TerrainBrush, {
    s32 value;
    s32 radius;
    // TODO: repeat modes
    // TODO: brush types
    // TODO: data layer
    // - TODO: bitmask brush for bitmask layers
});

ECS_STRUCT(DirtyChunkBuffer, {
    u32 count;
    u32 *chunks;
});



/* Rendering */

ECS_STRUCT(WindowSize, {
    float x;
    float y;
});

// Pixel coordinates formatted for shader uniform use; origin at bottom-left, lie on half integer boundary (+0.5, +0.5)
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

extern ECS_TAG_DECLARE(VertexShaderSource);
extern ECS_TAG_DECLARE(FragmentShaderSource);

// NOTE: could also set this up as 2 different basaltic_components
ECS_STRUCT(PipelineDescription, {
    sg_pipeline_desc *pipeline_desc;
    sg_shader_desc *shader_desc;
});

ECS_STRUCT(Pipeline, {
    sg_pipeline pipeline;
    sg_shader shader;
});

// Relationship where the target is a Pipeline entity
extern ECS_TAG_DECLARE(RenderPipeline);

extern ECS_TAG_DECLARE(TerrainRender);
extern ECS_TAG_DECLARE(DebugRender);

ECS_STRUCT(WrapInstanceOffsets, {
    vec3 offsets[4];
});

// TODO: would be nice if this was resizable
ECS_STRUCT(InstanceBuffer, {
    s32 maxInstances;
    s32 instances;
    ECS_PRIVATE;
    size_t size;
    void *data;
    sg_buffer buffer;
});

/** Immutable buffers */
ECS_STRUCT(Mesh, {
    sg_buffer vertexBuffers[SG_MAX_VERTEX_BUFFERS - 1]; // 0th vertex buffer reserved for instance buffer
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

// Immutable texture array, all get bound to the fragment stage
ECS_STRUCT(Texture, {
    sg_image images[SG_MAX_SHADERSTAGE_IMAGES];
});

// Mutable texture
ECS_STRUCT(DataTexture, {
    sg_image image;
    sg_sampler sampler;
    u32 width;
    u32 height;
    void *data;
    size_t size;
    size_t formatSize; // Unfortunately Sokol doesn't provide a way to get size from format, so have to compute it by hand
});

// TODO: purpose of this has changed since removing SSBO. Consider renaming or moving this data
// NOTE: can tell parser to ignore members after a certain line by adding ECS_PRIVATE
ECS_STRUCT(TerrainBuffer, {
    // Radius of visible chunks around the camera center. 1 = only chunk containing camera is visible; 2 = 3x3 area; 3 = 5x5 area; etc.
    u32 renderedChunkRadius;
    u32 renderedChunkCount;
    // length renderedChunkCount arrays that are compared to determine what chunks to reload each frame, and where to place them
    s32 *closestChunks;
    s32 *loadedChunks;
    vec3 *chunkPositions;
});



void BcviewImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_VIEW_H_INCLUDED
