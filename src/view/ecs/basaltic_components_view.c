#define BASALTIC_VIEW_IMPL
#include "basaltic_components_view.h"
#include "assert.h"

ECS_COMPONENT_DECLARE(s32);
ECS_COMPONENT_DECLARE(vec3);

ECS_COMPONENT_DECLARE(Scale);
ECS_COMPONENT_DECLARE(Color);

ECS_COMPONENT_DECLARE(ModelLastRenderedStep);

ECS_TAG_DECLARE(Previous);

ECS_TAG_DECLARE(VertexShaderSource);
ECS_TAG_DECLARE(FragmentShaderSource);

ECS_TAG_DECLARE(RenderPipeline);
ECS_TAG_DECLARE(TerrainRender);
ECS_TAG_DECLARE(DebugRender);

// TODO: consider rearranging module paths. It would be convenient if the view and model were at the same depth, with different top level names
void BcviewImport(ecs_world_t *world) {
    ECS_MODULE(world, Bcview);

    // NOTE: Must have this import before any other definitions, to keep component IDs consistent between model and view
    ECS_IMPORT(world, Bc);

    // Creating meta info for custom primitive types:
    // 1. Declare and Define a component for the type
    // 2. Use ecs_primitive to create meta info
    ECS_COMPONENT_DEFINE(world, s32);

    ecs_primitive(world, {.entity = ecs_id(s32), .kind = EcsI32});
    // NOTE: because Flecs also happens to use the name u32 for uint32_t prims, I don't need to create reflection data for it
    //ecs_primitive(world, {.entity = ecs_id(u32), .kind = EcsU32});

    // Create one ecs_member_t array per vector type, reuse for each component type
    ecs_member_t vec3Members[3] = {
        [0] = {.name = "x", .type = ecs_id(ecs_f32_t)},
        [1] = {.name = "y", .type = ecs_id(ecs_f32_t)},
        [2] = {.name = "z", .type = ecs_id(ecs_f32_t)},
    };

    ECS_COMPONENT_DEFINE(world, vec3);
    ecs_struct(world, {.entity = ecs_id(vec3), .members = {
        [0] = vec3Members[0],
        [1] = vec3Members[1],
        [2] = vec3Members[2]
    }});

    // TODO: figure out how to re-use vec3's struct_desc for Scale
    ECS_COMPONENT_DEFINE(world, Scale);
    ECS_COMPONENT_DEFINE(world, Color);

    ECS_META_COMPONENT(world, ResourceFile);

    ECS_META_COMPONENT(world, ModelWorld);
    ECS_META_COMPONENT(world, ModelQuery);
    ECS_COMPONENT_DEFINE(world, QueryDesc);
    ECS_COMPONENT_DEFINE(world, ModelLastRenderedStep);

    ECS_TAG_DEFINE(world, Previous);
    ECS_META_COMPONENT(world, DeltaTime);
    ECS_META_COMPONENT(world, Pointer);
    ECS_META_COMPONENT(world, Camera);
    ECS_META_COMPONENT(world, CameraWrap);
    ECS_META_COMPONENT(world, CameraSpeed);
    ECS_META_COMPONENT(world, RenderDistance);
    ECS_META_COMPONENT(world, FocusPlane);
    ECS_META_COMPONENT(world, HoveredCell);
    ECS_META_COMPONENT(world, SelectedCell);
    ECS_META_COMPONENT(world, TerrainBrush);
    ECS_META_COMPONENT(world, DirtyChunkBuffer);

    ECS_META_COMPONENT(world, WindowSize);
    ECS_META_COMPONENT(world, Mouse);
    ECS_COMPONENT_DEFINE(world, PVMatrix);
    ECS_COMPONENT_DEFINE(world, ModelMatrix);
    ECS_META_COMPONENT(world, Clock);

    ECS_TAG_DEFINE(world, VertexShaderSource);
    ECS_TAG_DEFINE(world, FragmentShaderSource);

    ECS_COMPONENT_DEFINE(world, PipelineDescription);
    ECS_COMPONENT_DEFINE(world, Pipeline);
    ECS_TAG_DEFINE(world, RenderPipeline);
    ecs_add_id(world, RenderPipeline, EcsTraversable);
    ecs_add_id(world, RenderPipeline, EcsOneOf);
    ecs_add_id(world, RenderPipeline, EcsExclusive);

    ECS_TAG_DEFINE(world, TerrainRender);
    ECS_TAG_DEFINE(world, DebugRender);

    ECS_COMPONENT_DEFINE(world, WrapInstanceOffsets);
    ECS_META_COMPONENT(world, InstanceBuffer);
    ECS_COMPONENT_DEFINE(world, Mesh);
    ECS_META_COMPONENT(world, Elements);

    ECS_COMPONENT_DEFINE(world, FeedbackBuffer);
    ECS_COMPONENT_DEFINE(world, Texture);
    ECS_COMPONENT_DEFINE(world, DataTexture);
    ECS_COMPONENT_DEFINE(world, TerrainBuffer);

    //ecs_singleton_add(world, ModelWorld);
    ecs_singleton_set(world, ModelLastRenderedStep, {0});

    ecs_singleton_set(world, Pointer, {0});

    ecs_singleton_set(world, Camera, {
        .origin = {{0, 0, 0}},
        .distance = 10.0f,
        .pitch = 45.0f,
        .yaw = 0.0f
    });

    ecs_singleton_set(world, CameraSpeed, {
        .movement = 10.0f,
        .rotation = 90.0f
    });

    // Global scale for world rendering
    ecs_singleton_set(world, Scale, {{1.0, 1.0, 0.1}});

    // Set later with bc_SetCameraWrapLimits
    ecs_singleton_add(world, CameraWrap);
    ecs_singleton_add(world, WrapInstanceOffsets);

    ecs_singleton_set(world, RenderDistance, {.radius = 2});
    ecs_singleton_set(world, FocusPlane, {0});
    ecs_singleton_set(world, HoveredCell, {0});
    ecs_singleton_set(world, SelectedCell, {0});
    ecs_singleton_set(world, TerrainBrush, {.value = 1, .radius = 1});
    ecs_singleton_set(world, DirtyChunkBuffer, {.count = 0, .chunks = calloc(256, sizeof(s32))}); // TODO: should be sized according to FocusPlane chunk count

    // Input
    ecs_singleton_add(world, Pointer);
    ecs_add_pair(world, ecs_id(Pointer), Previous, ecs_id(HoveredCell));

    // Global uniforms
    ecs_singleton_add(world, DeltaTime);
    ecs_singleton_add(world, Mouse);
    ecs_singleton_add(world, PVMatrix);
    ecs_singleton_add(world, Clock);

    /* Renderer Uniforms */
    //ecs_entity_t window;
}
