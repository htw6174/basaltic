#include "basaltic_phases_view.h"

ECS_TAG_DECLARE(BeginPassShadow);
ECS_TAG_DECLARE(OnPassShadow);
ECS_TAG_DECLARE(EndPassShadow);

ECS_TAG_DECLARE(BeginPassGBuffer);
ECS_TAG_DECLARE(OnPassGBuffer);
ECS_TAG_DECLARE(EndPassGBuffer);

ECS_TAG_DECLARE(BeginPassLighting);
ECS_TAG_DECLARE(OnPassLighting);
ECS_TAG_DECLARE(EndPassLighting);

ECS_TAG_DECLARE(BeginPassTransparent);
ECS_TAG_DECLARE(OnPassTransparent);
ECS_TAG_DECLARE(EndPassTransparent);

ECS_TAG_DECLARE(OnPassFinal);

ECS_TAG_DECLARE(OnModelChanged);

ECS_DECLARE(ModelChangedPipeline);

// returns a dummy phase with the same depth as endPhase; should use this as the dependsOn arg of further calls or phases
ecs_entity_t createPassPhases(ecs_world_t *world, ecs_entity_t dependsOn, ecs_entity_t beginPhase, ecs_entity_t onPhase, ecs_entity_t endPhase);

ecs_entity_t createPassPhases(ecs_world_t *world, ecs_entity_t dependsOn, ecs_entity_t beginPhase, ecs_entity_t onPhase, ecs_entity_t endPhase) {
    // Dummy phases to increase depth of later pass phases
    ecs_entity_t d1 = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t d2 = ecs_new_w_id(world, EcsPhase);
    ecs_entity_t d3 = ecs_new_w_id(world, EcsPhase);
    ecs_add_pair(world, d1, EcsDependsOn, dependsOn);
    ecs_add_pair(world, d2, EcsDependsOn, d1);
    ecs_add_pair(world, d3, EcsDependsOn, d2);

    // Ensure phase tag is added to args
    ecs_add_id(world, beginPhase, EcsPhase);
    ecs_add_id(world, onPhase, EcsPhase);
    ecs_add_id(world, endPhase, EcsPhase);

    // Ending a pass only depends on starting the same pass, not on running it. However, OnPass phases always need to happen in between starting and ending the same pass
    // Obviously, running a pass still directly depends on starting the same pass
    ecs_add_pair(world, beginPhase, EcsDependsOn, dependsOn);
    // dummy phase with same depth as onPhase
    ecs_entity_t dummyOnPass = ecs_new_w_id(world, EcsPhase);
    ecs_add_pair(world, dummyOnPass, EcsDependsOn, beginPhase);
    ecs_add_pair(world, onPhase, EcsDependsOn, beginPhase);
    ecs_add_pair(world, endPhase, EcsDependsOn, dummyOnPass);

    return d3;
}

void BcviewPhasesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewPhases);

    ECS_TAG_DEFINE(world, BeginPassShadow);
    ECS_TAG_DEFINE(world, OnPassShadow);
    ECS_TAG_DEFINE(world, EndPassShadow);

    ECS_TAG_DEFINE(world, BeginPassGBuffer);
    ECS_TAG_DEFINE(world, OnPassGBuffer);
    ECS_TAG_DEFINE(world, EndPassGBuffer);

    ECS_TAG_DEFINE(world, BeginPassLighting);
    ECS_TAG_DEFINE(world, OnPassLighting);
    ECS_TAG_DEFINE(world, EndPassLighting);

    ECS_TAG_DEFINE(world, BeginPassTransparent);
    ECS_TAG_DEFINE(world, OnPassTransparent);
    ECS_TAG_DEFINE(world, EndPassTransparent);

    ECS_TAG_DEFINE(world, OnPassFinal);

    ECS_TAG_DEFINE(world, OnModelChanged);

    ecs_entity_t dummyPhase = createPassPhases(world, EcsOnUpdate, BeginPassShadow, OnPassShadow, EndPassShadow);
    dummyPhase = createPassPhases(world, dummyPhase, BeginPassGBuffer, OnPassGBuffer, EndPassGBuffer);
    dummyPhase = createPassPhases(world, dummyPhase, BeginPassLighting, OnPassLighting, EndPassLighting);
    dummyPhase = createPassPhases(world, dummyPhase, BeginPassTransparent, OnPassTransparent, EndPassTransparent);

    ecs_add_id(world, OnPassFinal, EcsPhase);
    ecs_add_pair(world, OnPassFinal, EcsDependsOn, dummyPhase);

    // We don't want the default pipeline to run this phase, so don't add the EcsPhase tag
    ModelChangedPipeline = ecs_pipeline(world, {
        .query.filter.terms = {
            { .id = EcsSystem }, // Mandatory, must always match systems
            { .id = OnModelChanged } // EcsOr in all the phase tags you want this pipeline to run
        }
    });
}
