#include "basaltic_components_actors.h"
#define BC_COMPONENT_IMPL
#include "basaltic_components_wildlife.h"

void BcWildlifeImport(ecs_world_t *world) {
    ECS_MODULE(world, BcWildlife);

    ECS_IMPORT(world, BcActors);

    ECS_TAG_DEFINE(world, Diet);
    ecs_add_id(world, Diet, EcsOneOf);

    ECS_TAG_DEFINE(world, Grasses);
    ecs_add_pair(world, Grasses, EcsChildOf, Diet);
    ECS_TAG_DEFINE(world, Foliage);
    ecs_add_pair(world, Foliage, EcsChildOf, Diet);
    ECS_TAG_DEFINE(world, Fruit);
    ecs_add_pair(world, Fruit, EcsChildOf, Diet);
    ECS_TAG_DEFINE(world, Meat);
    ecs_add_pair(world, Meat, EcsChildOf, Diet);

    ECS_TAG_DEFINE(world, EgoGrazer);
    ecs_add_pair(world, EgoGrazer, EcsChildOf, Ego);
    ECS_TAG_DEFINE(world, EgoPredator);
    ecs_add_pair(world, EgoPredator, EcsChildOf, Ego);
    ECS_TAG_DEFINE(world, EgoHunter);
    ecs_add_pair(world, EgoHunter, EcsChildOf, Ego);

    ECS_TAG_DEFINE(world, ActionFeed);
    ecs_add_pair(world, ActionFeed, EcsChildOf, Action);
    ECS_TAG_DEFINE(world, ActionFollow);
    ecs_add_pair(world, ActionFollow, EcsChildOf, Action);
    ECS_TAG_DEFINE(world, ActionAttack);
    ecs_add_pair(world, ActionAttack, EcsChildOf, Action);

    ECS_TAG_DEFINE(world, Flying);
    ECS_TAG_DEFINE(world, Climbing);
    ECS_TAG_DEFINE(world, Amphibious);
    ECS_TAG_DEFINE(world, Aquatic);
}
