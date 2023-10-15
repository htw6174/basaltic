#include "basaltic_components_actors.h"
#define BC_COMPONENT_IMPL
#include "bc_components_elementals.h"

void BcElementalsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcElementals);

    ECS_IMPORT(world, BcActors);

    ECS_TAG_DEFINE(world, ElementalSpirit);

    ECS_TAG_DEFINE(world, EgoStormSpirit);
    ECS_TAG_DEFINE(world, EgoEarthSpirit);
    ECS_TAG_DEFINE(world, EgoOceanSpirit);
    ECS_TAG_DEFINE(world, EgoVolcanoSpirit);
    ecs_add_pair(world, EgoVolcanoSpirit, EcsChildOf, Ego);

    ECS_TAG_DEFINE(world, ActionErupt);
    ecs_add_pair(world, ActionErupt, EcsChildOf, Action);

    ECS_META_COMPONENT(world, SpiritPower);
}
