#define BASALTIC_COMPONENTS_IMPL
#include "basaltic_components.h"
#include "basaltic_phases.h"
#include "flecs.h"

ECS_COMPONENT_DECLARE(s16);

ECS_TAG_DECLARE(PlayerVision);

void BcImport(ecs_world_t *world) {
    ECS_MODULE(world, Bc);

    ECS_COMPONENT_DEFINE(world, s16);
    ECS_META_COMPONENT(world, Step);

    ecs_primitive(world, {.entity = ecs_id(s16), .kind = EcsI16});

    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);
    ECS_IMPORT(world, BcActors);
    ECS_IMPORT(world, BcWildlife);

    ECS_TAG_DEFINE(world, PlayerVision);

    ecs_singleton_set(world, Step, {.step = 0});

    // NOTE: the 'EcsWith' relationship makes adding the source component automatically add the target component
    //ecs_add_pair(world, PlayerControlled, EcsWith, PlayerVision);
}
