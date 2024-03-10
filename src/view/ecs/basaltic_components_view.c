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

    ecs_member_t gridCoordMembers[2] = {
        [0] = {.name = "x", .type = ecs_id(ecs_i32_t)},
        [1] = {.name = "y", .type = ecs_id(ecs_i32_t)}
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

    ECS_COMPONENT_DEFINE(world, HoveredCell);
    ecs_struct(world, {.entity = ecs_id(HoveredCell), .members = {
        [0] = gridCoordMembers[0],
        [1] = gridCoordMembers[1]
    }});

    ECS_COMPONENT_DEFINE(world, SelectedCell);
    ecs_struct(world, {.entity = ecs_id(SelectedCell), .members = {
        [0] = gridCoordMembers[0],
        [1] = gridCoordMembers[1]
    }});


    ECS_META_COMPONENT(world, ModelWorld);
    ECS_META_COMPONENT(world, ModelQuery);
    ECS_META_COMPONENT(world, QueryDesc);
    //ECS_COMPONENT_DEFINE(world, ModelLastRenderedStep);

    // Interface
    ECS_TAG_DEFINE(world, Previous);

    ECS_META_COMPONENT(world, DeltaTime);
    ECS_META_COMPONENT(world, MousePreferences);
    ECS_META_COMPONENT(world, Camera);
    ECS_META_COMPONENT(world, CameraWrap);
    ECS_META_COMPONENT(world, CameraSpeed);
    ECS_META_COMPONENT(world, FocusPlane);
    ECS_META_COMPONENT(world, FocusEntity);
    ECS_META_COMPONENT(world, PlayerEntity);
    ECS_META_COMPONENT(world, DirtyChunkBuffer);

    ECS_TAG_DEFINE(world, Tool);
    ECS_TAG_DEFINE(world, BrushField);
    ecs_add_id(world, BrushField, EcsExclusive);
    ECS_META_COMPONENT(world, BrushSize);
    ECS_META_COMPONENT(world, AdditiveBrush);
    ecs_add_id(world, ecs_id(AdditiveBrush), Tool);
    ECS_META_COMPONENT(world, SmoothBrush);
    ecs_add_id(world, ecs_id(SmoothBrush), Tool);
    ECS_META_COMPONENT(world, ValueBrush);
    ecs_add_id(world, ecs_id(ValueBrush), Tool);
    ECS_META_COMPONENT(world, RiverBrush);
    ecs_add_id(world, ecs_id(RiverBrush), Tool);
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

    // Uniform Data
    ECS_META_COMPONENT(world, WindowSize);
    ECS_META_COMPONENT(world, ScaledCursor);
    ECS_META_COMPONENT(world, Visibility);
    ECS_COMPONENT_DEFINE(world, PVMatrix);
    ECS_COMPONENT_DEFINE(world, SunMatrix);
    ECS_COMPONENT_DEFINE(world, ModelMatrix);
    ECS_COMPONENT_DEFINE(world, InverseMatrices);
    ECS_META_COMPONENT(world, Clock);
    ECS_META_COMPONENT(world, SunLight);

    // Uniforms
    ECS_COMPONENT_DEFINE(world, UniformBlockDescription);
    ECS_COMPONENT_DEFINE(world, GlobalUniformsVert);
    ECS_META_COMPONENT(world, GlobalUniformsFrag);
    ECS_META_COMPONENT(world, TerrainPipelineUniformsVert);
    ECS_META_COMPONENT(world, TerrainPipelineUniformsFrag);
    ECS_COMPONENT_DEFINE(world, CommonDrawUniformsVert);

    // Rendering
    ECS_TAG_DEFINE(world, ShadowPass);
    ECS_TAG_DEFINE(world, GBufferPass);
    ECS_TAG_DEFINE(world, LightingPass);
    ECS_TAG_DEFINE(world, TransparentPass);
    ECS_TAG_DEFINE(world, FinalPass);
    ECS_COMPONENT_DEFINE(world, RenderPassDescription);
    ECS_COMPONENT_DEFINE(world, RenderPass);
    ECS_COMPONENT_DEFINE(world, RenderTarget);

    ecs_add_id(world, ShadowPass, EcsTraversable);
    ecs_add_id(world, ShadowPass, EcsOneOf);

    ecs_add_id(world, GBufferPass, EcsTraversable);
    ecs_add_id(world, GBufferPass, EcsOneOf);

    ecs_add_id(world, LightingPass, EcsTraversable);
    ecs_add_id(world, LightingPass, EcsOneOf);

    ecs_add_id(world, TransparentPass, EcsTraversable);
    ecs_add_id(world, TransparentPass, EcsOneOf);

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
    ecs_add_id(world, ecs_id(InstanceBuffer), EcsAlwaysOverride);
    ECS_META_COMPONENT(world, TerrainChunkInstanceData);
    ECS_META_COMPONENT(world, DebugInstanceData);
    ECS_META_COMPONENT(world, ArrowInstanceData);
    ECS_COMPONENT_DEFINE(world, Mesh);
    ECS_META_COMPONENT(world, Elements);

    ECS_COMPONENT_DEFINE(world, RingBuffer);
    ECS_COMPONENT_DEFINE(world, Texture);
    ECS_COMPONENT_DEFINE(world, DataTexture);
    ECS_COMPONENT_DEFINE(world, TerrainBuffer);

    /* Setup singleton defaults */
    // TODO: most of the singleton initialization moved to script. Play around with it, figure out if the rest can be moved / should be returned
    // NOTE: a potential issue with singleton setup here or in scripts is ordering with observers: don't want to create an entity before observers its components rely on for setup are defined. General order of operations should be component definitions, system definitions, entity creation

    // Set later with bc_SetCameraWrapLimits
    ecs_singleton_add(world, CameraWrap);
    ecs_singleton_add(world, WrapInstanceOffsets);

    ecs_singleton_set(world, FocusPlane, {0});
    ecs_singleton_set(world, FocusEntity, {0});
    ecs_singleton_set(world, PlayerEntity, {0});
    ecs_singleton_set(world, HoveredCell, {0});
    ecs_singleton_set(world, SelectedCell, {0});

    ecs_singleton_add(world, GlobalUniformsVert);
    ecs_singleton_add(world, GlobalUniformsFrag);
    // Uniform block descriptions
    ecs_set(world, ecs_id(GlobalUniformsVert), UniformBlockDescription, {
        .size = sizeof(GlobalUniformsVert),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "pv", .type = SG_UNIFORMTYPE_MAT4},
        }
    });
    ecs_set(world, ecs_id(GlobalUniformsFrag), UniformBlockDescription, {
        .size = sizeof(GlobalUniformsFrag),
            .layout = SG_UNIFORMLAYOUT_NATIVE,
            .uniforms = {
                [0] = {.name = "time", .type = SG_UNIFORMTYPE_FLOAT},
                [1] = {.name = "mouse", .type = SG_UNIFORMTYPE_FLOAT2},
            }
    });
    ecs_set(world, ecs_id(CommonDrawUniformsVert), UniformBlockDescription, {
        .size = sizeof(CommonDrawUniformsVert),
            .layout = SG_UNIFORMLAYOUT_NATIVE,
            .uniforms = {
                [0] = {.name = "m", .type = SG_UNIFORMTYPE_MAT4},
            }
    });

    ecs_set(world, ecs_id(TerrainPipelineUniformsVert), UniformBlockDescription, {
        .size = sizeof(TerrainPipelineUniformsVert),
            .layout = SG_UNIFORMLAYOUT_NATIVE,
            .uniforms = {
                [0] = {.name = "visibilityOverride", .type = SG_UNIFORMTYPE_INT},
            }
    });
    ecs_set(world, ecs_id(TerrainPipelineUniformsFrag), UniformBlockDescription, {
        .size = sizeof(TerrainPipelineUniformsFrag),
            .layout = SG_UNIFORMLAYOUT_NATIVE,
            .uniforms = {
                [0] = {.name = "drawBorders", .type = SG_UNIFORMTYPE_INT},
            }
    });

    ecs_singleton_set(world, DirtyChunkBuffer, {.count = 0, .chunks = calloc(256, sizeof(s32))}); // TODO: should be sized according to FocusPlane chunk count, should have a component ctor/dtor if it need to alloc

    // Input
    ecs_add_pair(world, ecs_id(HoveredCell), Previous, ecs_id(HoveredCell));

    // Global uniforms
    ecs_singleton_add(world, PVMatrix);
    ecs_singleton_add(world, InverseMatrices);
    ecs_singleton_add(world, DeltaTime);
    ecs_singleton_add(world, ScaledCursor);
    ecs_singleton_add(world, Visibility);
    ecs_singleton_add(world, Clock);
}
