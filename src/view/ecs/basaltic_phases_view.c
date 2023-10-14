#include "basaltic_phases_view.h"

ECS_TAG_DECLARE(BeginPassShadow);
ECS_TAG_DECLARE(OnPassShadow);
ECS_TAG_DECLARE(EndPassShadow);

ECS_TAG_DECLARE(BeginPassGBuffer);
ECS_TAG_DECLARE(OnPassGBuffer);
ECS_TAG_DECLARE(EndPassGBuffer);

ECS_TAG_DECLARE(OnPassLighting);
ECS_TAG_DECLARE(OnPassFinal);

ECS_TAG_DECLARE(OnModelChanged);

ECS_DECLARE(ModelChangedPipeline);

void BcviewPhasesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewPhases);

    ECS_TAG_DEFINE(world, BeginPassShadow);
    ECS_TAG_DEFINE(world, OnPassShadow);
    ECS_TAG_DEFINE(world, EndPassShadow);

    ECS_TAG_DEFINE(world, BeginPassGBuffer);
    ECS_TAG_DEFINE(world, OnPassGBuffer);
    ECS_TAG_DEFINE(world, EndPassGBuffer);

    ECS_TAG_DEFINE(world, OnPassLighting);
    ECS_TAG_DEFINE(world, OnPassFinal);

    ECS_TAG_DEFINE(world, OnModelChanged);

    // TODO: change the relationship between pass phases. Ending a pass only depends on starting the same pass, not on running it. However, OnPass phases always need to happen in between starting and ending the same pass
    // Obviously, running a pass still directly depends on starting the same pass

    ecs_add_id(world, BeginPassShadow, EcsPhase);
    ecs_add_pair(world, BeginPassShadow, EcsDependsOn, EcsOnUpdate);
    // dummy phase with same depth as OnShadowPass
    ecs_entity_t whileShadowPass = ecs_new_w_id(world, EcsPhase);
    ecs_add_pair(world, whileShadowPass, EcsDependsOn, BeginPassShadow);
    ecs_add_id(world, OnPassShadow, EcsPhase);
    ecs_add_pair(world, OnPassShadow, EcsDependsOn, BeginPassShadow);
    ecs_add_id(world, EndPassShadow, EcsPhase);
    ecs_add_pair(world, EndPassShadow, EcsDependsOn, whileShadowPass);

    // Dummy phases to increase depth of post-shadow-pass phases
    ecs_entity_t d1 = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t d2 = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t d3 = ecs_new_w_id(world, EcsPhase);
    ecs_add_pair(world, d1, EcsDependsOn, EcsOnUpdate);
    ecs_add_pair(world, d2, EcsDependsOn, d1);
    ecs_add_pair(world, d3, EcsDependsOn, d2);

    // TEST: default disable shadows
    //ecs_enable(world, OnPassShadow, false);

    ecs_add_id(world, BeginPassGBuffer, EcsPhase);
    ecs_add_pair(world, BeginPassGBuffer, EcsDependsOn, d3);
    // dummy phase with same depth as OnRenderPass
    ecs_entity_t whileGBufferPass = ecs_new_w_id(world, EcsPhase);
    ecs_add_pair(world, whileGBufferPass, EcsDependsOn, BeginPassGBuffer);
    ecs_add_id(world, OnPassGBuffer, EcsPhase);
    ecs_add_pair(world, OnPassGBuffer, EcsDependsOn, BeginPassGBuffer);
    ecs_add_id(world, EndPassGBuffer, EcsPhase);
    ecs_add_pair(world, EndPassGBuffer, EcsDependsOn, whileGBufferPass);

    // Could do another 3 dummy phases to span d3 to OnLighting, which would allow disabling the gbuffer pass
    // Might be useful for a more generalized system, but not worth setting up for now

    ecs_add_id(world, OnPassLighting, EcsPhase);
    ecs_add_pair(world, OnPassLighting, EcsDependsOn, EndPassGBuffer);

    ecs_add_id(world, OnPassFinal, EcsPhase);
    ecs_add_pair(world, OnPassFinal, EcsDependsOn, OnPassLighting);

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
