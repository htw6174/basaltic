#ifndef BASALTIC_COMPONENTS_VIEW_H_INCLUDED
#define BASALTIC_COMPONENTS_VIEW_H_INCLUDED

#include "htw_core.h"
#include "ccVector.h"
#include "sokol_gfx.h"
#include "flecs.h"

#undef ECS_META_IMPL
#ifndef BASALTIC_VIEW_IMPL
#define ECS_META_IMPL EXTERN // Ensure meta symbols are only defined once
#endif

/* Common types */
extern ECS_COMPONENT_DECLARE(u32);
extern ECS_COMPONENT_DECLARE(s32);



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

ECS_STRUCT(FocusPlane, {
    ecs_entity_t entity;
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

ECS_STRUCT(Binding, {
    sg_bindings binding;
    s32 elements;
    s32 instances;
});

extern ECS_TAG_DECLARE(TerrainRender);



void BasalticComponentsViewImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_VIEW_H_INCLUDED
