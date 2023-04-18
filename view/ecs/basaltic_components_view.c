#define BASALTIC_VIEW_IMPL
#include "basaltic_components_view.h"

ECS_COMPONENT_DECLARE(u32);
ECS_COMPONENT_DECLARE(s32);

ECS_TAG_DECLARE(TerrainRender);

void BasalticComponentsViewImport(ecs_world_t *world) {
    ECS_MODULE(world, BasalticComponentsView);

    // TODO: figure out correct way to create reflection data for custom primitive types used in other components
    //printf("%lu\n", ecs_id(ecs_i32_t));
    //ecs_primitive(world, {.entity = ecs_id(ecs_i32_t), .kind = EcsI32});
    //ECS_COMPONENT_DEFINE(world, u32);
    //ECS_COMPONENT_DEFINE(world, s32);
    //ecs_primitive(world, {.entity = ecs_id(u32), .kind = EcsU32});
    //ecs_primitive(world, {.entity = ecs_id(s32), .kind = EcsI32});

    ECS_META_COMPONENT(world, ModelWorld);
    ECS_META_COMPONENT(world, ModelQuery);
    ECS_COMPONENT_DEFINE(world, QueryDesc);

    ECS_COMPONENT_DEFINE(world, Pointer);
    ECS_COMPONENT_DEFINE(world, Camera);
    ECS_META_COMPONENT(world, CameraWrap);
    ECS_META_COMPONENT(world, CameraSpeed);
    ECS_COMPONENT_DEFINE(world, RenderDistance);
    ECS_META_COMPONENT(world, FocusPlane);
    ECS_COMPONENT_DEFINE(world, HoveredCell);

    ECS_META_COMPONENT(world, WindowSize);
    ECS_META_COMPONENT(world, Mouse);
    ECS_COMPONENT_DEFINE(world, PVMatrix);
    ECS_COMPONENT_DEFINE(world, ModelMatrix);
    ECS_COMPONENT_DEFINE(world, Pipeline);
    ECS_COMPONENT_DEFINE(world, Binding);

    ECS_TAG_DEFINE(world, TerrainRender);

    ECS_COMPONENT_DEFINE(world, WrapInstanceOffsets);
    ECS_COMPONENT_DEFINE(world, FeedbackBuffer);
    ECS_COMPONENT_DEFINE(world, TerrainBuffer);

    //ecs_singleton_add(world, ModelWorld);

    ecs_singleton_set(world, Pointer, {0});

    ecs_singleton_set(world, Camera, {
        .origin = {{0, 0, 0}},
        .distance = 10.0f,
        .pitch = 45.0f,
        .yaw = 0.0f
    });

    ecs_singleton_set(world, CameraSpeed, {
        .movement = 0.15f,
        .rotation = 1.5f
    });

    // Set later with bc_SetCameraWrapLimits
    ecs_singleton_add(world, CameraWrap);
    ecs_singleton_add(world, WrapInstanceOffsets);

    ecs_singleton_set(world, RenderDistance, {.radius = 2});
    ecs_singleton_set(world, FocusPlane, {0});
    ecs_singleton_set(world, HoveredCell, {0});

    // Global uniforms
    ecs_singleton_add(world, Mouse);
    ecs_singleton_add(world, PVMatrix);

    /* Renderer Uniforms */
    //ecs_entity_t window;
}
