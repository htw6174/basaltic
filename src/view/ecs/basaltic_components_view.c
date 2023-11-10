#define BASALTIC_VIEW_IMPL
#include "basaltic_components_view.h"

// TODO: consider rearranging module paths. It would be convenient if the view and model were at the same depth, with different top level names
void BcviewImport(ecs_world_t *world) {
    ECS_MODULE(world, Bcview);

    // NOTE: Must have this import before any other definitions, to keep component IDs consistent between model and view
    ECS_IMPORT(world, Bc);

    // Create one ecs_member_t array per vector type, reuse for each component type
    ecs_member_t vecMembers[4] = {
        [0] = {.name = "x", .type = ecs_id(ecs_f32_t)},
        [1] = {.name = "y", .type = ecs_id(ecs_f32_t)},
        [2] = {.name = "z", .type = ecs_id(ecs_f32_t)},
        [3] = {.name = "w", .type = ecs_id(ecs_f32_t)},
    };

    ecs_member_t colorMembers[4] = {
        [0] = {.name = "r", .type = ecs_id(ecs_f32_t)},
        [1] = {.name = "g", .type = ecs_id(ecs_f32_t)},
        [2] = {.name = "b", .type = ecs_id(ecs_f32_t)},
        [3] = {.name = "a", .type = ecs_id(ecs_f32_t)},
    };

    ECS_COMPONENT_DEFINE(world, vec2);
    ecs_struct(world, {.entity = ecs_id(vec2), .members = {
        [0] = vecMembers[0],
        [1] = vecMembers[1]
    }});

    ECS_COMPONENT_DEFINE(world, vec3);
    ecs_struct(world, {.entity = ecs_id(vec3), .members = {
        [0] = vecMembers[0],
        [1] = vecMembers[1],
        [2] = vecMembers[2]
    }});

    ECS_COMPONENT_DEFINE(world, vec4);
    ecs_struct(world, {.entity = ecs_id(vec4), .members = {
        [0] = vecMembers[0],
        [1] = vecMembers[1],
        [2] = vecMembers[2],
        [3] = vecMembers[3]
    }});

    ECS_COMPONENT_DEFINE(world, Scale);
    ecs_struct(world, {.entity = ecs_id(Scale), .members = {
        [0] = vecMembers[0],
        [1] = vecMembers[1],
        [2] = vecMembers[2]
    }});

    ECS_COMPONENT_DEFINE(world, Color);
    ecs_struct(world, {.entity = ecs_id(Color), .members = {
        [0] = colorMembers[0],
        [1] = colorMembers[1],
        [2] = colorMembers[2],
        [3] = colorMembers[3]
    }});

    ECS_COMPONENT_DEFINE(world, Rect);
    ecs_struct(world, {.entity = ecs_id(Rect), .members = {
        [0] = vecMembers[0],
        [1] = vecMembers[1],
        [2] = {.name = "width", .type = ecs_id(ecs_f32_t)},
        [3] = {.name = "height", .type = ecs_id(ecs_f32_t)}
    }});

    ECS_META_COMPONENT(world, ModelWorld);
    ECS_META_COMPONENT(world, ModelQuery);
    ECS_META_COMPONENT(world, QueryDesc);
    //ECS_COMPONENT_DEFINE(world, ModelLastRenderedStep);

    // Interface
    ECS_TAG_DEFINE(world, Previous);
    ECS_META_COMPONENT(world, MouseButton);
    ECS_META_COMPONENT(world, InputType);
    ECS_META_COMPONENT(world, InputMotion);
    ECS_META_COMPONENT(world, InputModifier);
    ECS_COMPONENT_DEFINE(world, KeyCode);
    ecs_primitive(world, {.entity = ecs_id(KeyCode), .kind = EcsI32});
    ECS_TAG_DEFINE(world, InputBindGroup);
    ECS_META_COMPONENT(world, InputBinding);

    ECS_META_COMPONENT(world, DeltaTime);
    ECS_META_COMPONENT(world, Pointer);
    ECS_META_COMPONENT(world, MousePreferences);
    ECS_META_COMPONENT(world, Camera);
    ECS_META_COMPONENT(world, CameraWrap);
    ECS_META_COMPONENT(world, CameraSpeed);
    ECS_META_COMPONENT(world, FocusPlane);
    ECS_META_COMPONENT(world, HoveredCell);
    ECS_META_COMPONENT(world, SelectedCell);
    ECS_META_COMPONENT(world, DirtyChunkBuffer);

    ECS_TAG_DEFINE(world, Tool);
    ECS_META_COMPONENT(world, TerrainBrush);
    ecs_add_id(world, ecs_id(TerrainBrush), Tool);
    ECS_META_COMPONENT(world, PrefabBrush);
    ecs_add_id(world, ecs_id(PrefabBrush), Tool);

    // Settings
    ECS_TAG_DEFINE(world, VideoSettings);
    ECS_COMPONENT_DEFINE(world, RenderScale);
    ecs_primitive(world, {.entity = ecs_id(RenderScale), .kind = EcsF32});
    // TEST: set meta info min and max to test with UI controls later
    ecs_set(world, ecs_id(RenderScale), EcsMemberRanges, {.value = {.min = 0.1, .max = 2.0}});
    ECS_COMPONENT_DEFINE(world, ShadowMapSize);
    ecs_primitive(world, {.entity = ecs_id(ShadowMapSize), .kind = EcsI32});
    ECS_META_COMPONENT(world, RenderDistance);

    // Rendering
    ECS_META_COMPONENT(world, WindowSize);
    ECS_META_COMPONENT(world, Mouse);
    ECS_COMPONENT_DEFINE(world, PVMatrix);
    ECS_COMPONENT_DEFINE(world, SunMatrix);
    ECS_COMPONENT_DEFINE(world, ModelMatrix);
    ECS_COMPONENT_DEFINE(world, InverseMatrices);
    ECS_META_COMPONENT(world, Clock);
    ECS_META_COMPONENT(world, SunLight);

    ECS_COMPONENT_DEFINE(world, RenderPassDescription);
    ECS_COMPONENT_DEFINE(world, RenderPass);
    ECS_COMPONENT_DEFINE(world, RenderTarget);
    ECS_TAG_DEFINE(world, ShadowPass);
    ECS_TAG_DEFINE(world, GBufferPass);
    ECS_TAG_DEFINE(world, LightingPass);
    ECS_TAG_DEFINE(world, FinalPass);

    ecs_add_id(world, ShadowPass, EcsTraversable);
    ecs_add_id(world, ShadowPass, EcsOneOf);

    ecs_add_id(world, GBufferPass, EcsTraversable);
    ecs_add_id(world, GBufferPass, EcsOneOf);

    ecs_add_id(world, LightingPass, EcsTraversable);
    ecs_add_id(world, LightingPass, EcsOneOf);

    ecs_add_id(world, FinalPass, EcsTraversable);
    ecs_add_id(world, FinalPass, EcsOneOf);


    ECS_TAG_DEFINE(world, VertexShaderSource);
    ECS_TAG_DEFINE(world, FragmentShaderSource);

    ECS_COMPONENT_DEFINE(world, PipelineDescription);
    ECS_COMPONENT_DEFINE(world, Pipeline);


    ECS_TAG_DEFINE(world, InternalRender);
    ECS_TAG_DEFINE(world, TerrainRender);
    ECS_TAG_DEFINE(world, DebugRender);

    ECS_COMPONENT_DEFINE(world, WrapInstanceOffsets);
    ECS_META_COMPONENT(world, InstanceBuffer);
    ECS_COMPONENT_DEFINE(world, Mesh);
    ECS_META_COMPONENT(world, Elements);

    ECS_COMPONENT_DEFINE(world, RingBuffer);
    ECS_COMPONENT_DEFINE(world, Texture);
    ECS_COMPONENT_DEFINE(world, DataTexture);
    ECS_COMPONENT_DEFINE(world, TerrainBuffer);

    /* Setup singleton defaults */
    // TODO: most of the singleton initialization moved to script. Play around with it, figure out if the rest can be moved / should be returned

    // Set later with bc_SetCameraWrapLimits
    ecs_singleton_add(world, CameraWrap);
    ecs_singleton_add(world, WrapInstanceOffsets);

    ecs_singleton_set(world, FocusPlane, {0});
    ecs_singleton_set(world, HoveredCell, {0});
    ecs_singleton_set(world, SelectedCell, {0});
    ecs_singleton_set(world, TerrainBrush, {.value = 1, .radius = 1});
    ecs_singleton_set(world, PrefabBrush, {.prefab = 0});

    ecs_singleton_set(world, DirtyChunkBuffer, {.count = 0, .chunks = calloc(256, sizeof(s32))}); // TODO: should be sized according to FocusPlane chunk count, should have a component ctor/dtor if it need to alloc

    // Input
    ecs_singleton_add(world, Pointer);
    ecs_add_pair(world, ecs_id(Pointer), Previous, ecs_id(HoveredCell));

    // Global uniforms
    ecs_singleton_add(world, DeltaTime);
    ecs_singleton_add(world, Mouse);
    ecs_singleton_add(world, Clock);
}
