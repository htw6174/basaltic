#include "basaltic_phases_view.h"

ECS_TAG_DECLARE(PreShadowPass);
ECS_TAG_DECLARE(OnShadowPass);
ECS_TAG_DECLARE(PostShadowPass);

ECS_TAG_DECLARE(PreRenderPass);
ECS_TAG_DECLARE(OnRenderPass);
ECS_TAG_DECLARE(PostRenderPass);

ECS_TAG_DECLARE(OnLightingPass);
ECS_TAG_DECLARE(OnFinalPass);

ECS_TAG_DECLARE(OnModelChanged);

ECS_DECLARE(ModelChangedPipeline);

void BcviewPhasesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewPhases);

    ECS_TAG_DEFINE(world, PreShadowPass);
    ECS_TAG_DEFINE(world, OnShadowPass);
    ECS_TAG_DEFINE(world, PostShadowPass);

    ECS_TAG_DEFINE(world, PreRenderPass);
    ECS_TAG_DEFINE(world, OnRenderPass);
    ECS_TAG_DEFINE(world, PostRenderPass);

    ECS_TAG_DEFINE(world, OnLightingPass);
    ECS_TAG_DEFINE(world, OnFinalPass);

    ECS_TAG_DEFINE(world, OnModelChanged);

    // TODO: change the relationship between pass phases. Ending a pass only depends on starting the same pass, not on running it. However, OnPass phases always need to happen in between starting and ending the same pass
    // Obviously, running a pass still directly depends on starting the same pass

    ecs_add_id(world, PreShadowPass, EcsPhase);
    ecs_add_pair(world, PreShadowPass, EcsDependsOn, EcsOnUpdate);
    // dummy phase with same depth as OnShadowPass
    ecs_entity_t whileShadowPass = ecs_new_w_id(world, EcsPhase);
    ecs_add_pair(world, whileShadowPass, EcsDependsOn, PreShadowPass);
    ecs_add_id(world, OnShadowPass, EcsPhase);
    ecs_add_pair(world, OnShadowPass, EcsDependsOn, PreShadowPass);
    ecs_add_id(world, PostShadowPass, EcsPhase);
    ecs_add_pair(world, PostShadowPass, EcsDependsOn, whileShadowPass);

    // Dummy phases to increase depth of post-shadow-pass phases
    ecs_entity_t d1 = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t d2 = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t d3 = ecs_new_w_id(world, EcsPhase);
    ecs_add_pair(world, d1, EcsDependsOn, EcsOnUpdate);
    ecs_add_pair(world, d2, EcsDependsOn, d1);
    ecs_add_pair(world, d3, EcsDependsOn, d2);

    // TEST: default disable shadows
    //ecs_enable(world, PreShadowPass, false);

    ecs_add_id(world, PreRenderPass, EcsPhase);
    ecs_add_pair(world, PreRenderPass, EcsDependsOn, d3);
    // dummy phase with same depth as OnRenderPass
    ecs_entity_t whileRenderPass = ecs_new_w_id(world, EcsPhase);
    ecs_add_pair(world, whileRenderPass, EcsDependsOn, PreRenderPass);
    ecs_add_id(world, OnRenderPass, EcsPhase);
    ecs_add_pair(world, OnRenderPass, EcsDependsOn, PreRenderPass);
    ecs_add_id(world, PostRenderPass, EcsPhase);
    ecs_add_pair(world, PostRenderPass, EcsDependsOn, whileRenderPass);

    // Could do another 3 dummy phases to span d3 to OnLighting, which would allow disabling the render pass
    // Might be useful for a more generalized system, but not worth setting up for now

    ecs_add_id(world, OnLightingPass, EcsPhase);
    ecs_add_pair(world, OnLightingPass, EcsDependsOn, PostRenderPass);

    ecs_add_id(world, OnFinalPass, EcsPhase);
    ecs_add_pair(world, OnFinalPass, EcsDependsOn, OnLightingPass);

    // We don't want the default pipeline to run this phase, so don't add the EcsPhase tag
    //ecs_add_id(world, OnModelChanged, EcsPhase);

    //ECS_ENTITY_DEFINE(world, ModelChangedPipeline);

    ModelChangedPipeline = ecs_pipeline(world, {
        .query.filter.terms = {
            { .id = EcsSystem }, // Mandatory, must always match systems
            { .id = OnModelChanged } // OR in all the phase tags you want this pipeline to run
        }
    });
}
