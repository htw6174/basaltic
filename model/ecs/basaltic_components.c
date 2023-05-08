#include "basaltic_components.h"
#include "flecs.h"
#include "basaltic_phases.h"

ECS_TAG_DECLARE(PlayerVision);

void BcImport(ecs_world_t *world) {
    ECS_MODULE(world, Bc);

    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);
    ECS_IMPORT(world, BcActors);

    assert(ecs_id(Position) != 0);

    ECS_TAG_DEFINE(world, PlayerVision);

    // NOTE: the 'EcsWith' relationship makes adding the source component automatically add the target component
    //ecs_add_pair(world, PlayerControlled, EcsWith, PlayerVision);
}
