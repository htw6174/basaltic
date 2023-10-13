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

    ecs_add_id(world, PreShadowPass, EcsPhase);
    ecs_add_pair(world, PreShadowPass, EcsDependsOn, EcsOnUpdate);
    ecs_add_id(world, OnShadowPass, EcsPhase);
    ecs_add_pair(world, OnShadowPass, EcsDependsOn, PreShadowPass);
    ecs_add_id(world, PostShadowPass, EcsPhase);
    ecs_add_pair(world, PostShadowPass, EcsDependsOn, OnShadowPass);

    ecs_add_id(world, PreRenderPass, EcsPhase);
    ecs_add_pair(world, PreRenderPass, EcsDependsOn, PostShadowPass);
    ecs_add_id(world, OnRenderPass, EcsPhase);
    ecs_add_pair(world, OnRenderPass, EcsDependsOn, PreRenderPass);
    ecs_add_id(world, PostRenderPass, EcsPhase);
    ecs_add_pair(world, PostRenderPass, EcsDependsOn, OnRenderPass);

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
