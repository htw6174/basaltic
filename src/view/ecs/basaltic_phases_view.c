#include "basaltic_phases_view.h"

ECS_TAG_DECLARE(OnModelChanged);

ECS_DECLARE(ModelChangedPipeline);

void BcviewPhasesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewPhases);

    ECS_TAG_DEFINE(world, OnModelChanged);

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
