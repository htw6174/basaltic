#include "basaltic_components_wildlife.h"
#include "basaltic_components_actors.h"

ECS_TAG_DECLARE(Diet);
ECS_TAG_DECLARE(Grasses);
ECS_TAG_DECLARE(Foliage);
ECS_TAG_DECLARE(Fruit);
ECS_TAG_DECLARE(Meat);

ECS_TAG_DECLARE(EgoGrazer);
ECS_TAG_DECLARE(EgoPredator);
ECS_TAG_DECLARE(EgoHunter);

ECS_TAG_DECLARE(ActionFeed);
ECS_TAG_DECLARE(ActionFollow);
ECS_TAG_DECLARE(ActionAttack);

ECS_TAG_DECLARE(Flying);
ECS_TAG_DECLARE(Climbing);
ECS_TAG_DECLARE(Amphibious);
ECS_TAG_DECLARE(Aquatic);

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
