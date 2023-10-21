#define BASALTIC_COMPONENTS_IMPL
#include "basaltic_components.h"
#include "basaltic_phases.h"
#include "flecs.h"

ECS_TAG_DECLARE(PlayerVision);

void BcImport(ecs_world_t *world) {
    ECS_MODULE(world, Bc);

    ECS_IMPORT(world, BcCommon);
    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);
    ECS_IMPORT(world, BcActors);
    ECS_IMPORT(world, BcWildlife);
    ECS_IMPORT(world, BcElementals);

    ECS_TAG_DEFINE(world, PlayerVision);

    // NOTE: the 'EcsWith' relationship makes adding the source component automatically add the target component
    //ecs_add_pair(world, PlayerControlled, EcsWith, PlayerVision);
}
