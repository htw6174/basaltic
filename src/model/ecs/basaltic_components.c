#include "flecs.h"
#include "basaltic_phases.h"
#include "basaltic_components.h"

ECS_TAG_DECLARE(PlayerVision);

void BcImport(ecs_world_t *world) {
    ECS_MODULE(world, Bc);

    ECS_IMPORT(world, BcCommon);
    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);
    ECS_IMPORT(world, BcActors);
    ECS_IMPORT(world, BcWildlife);
    ECS_IMPORT(world, BcElementals);
    ECS_IMPORT(world, BcTribes);
}
