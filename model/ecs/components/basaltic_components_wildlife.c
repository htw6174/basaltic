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

    /* Prefabs */

    // wolf pack
    ecs_entity_t wolfPackPrefab = ecs_set_name(world, 0, "WolfPackPrefab");
    ecs_add_id(world, wolfPackPrefab, EcsPrefab);
    ecs_add_pair(world, wolfPackPrefab, Ego, EgoPredator);
    ecs_add_pair(world, wolfPackPrefab, Diet, Meat);
    ecs_set(world, wolfPackPrefab, Group, {.count = 10});
    ecs_override(world, wolfPackPrefab, Group);
    ecs_set(world, wolfPackPrefab, ActorSize, {ACTOR_SIZE_AVERAGE});

    // bison herd
    ecs_entity_t bisonHerdPrefab = ecs_set_name(world, 0, "BisonHerdPrefab");
    ecs_add_id(world, bisonHerdPrefab, EcsPrefab);
    ecs_add_pair(world, bisonHerdPrefab, Ego, EgoGrazer);
    ecs_add_pair(world, bisonHerdPrefab, Diet, Grasses);
    ecs_set(world, bisonHerdPrefab, Group, {.count = 30});
    ecs_override(world, bisonHerdPrefab, Group);
    ecs_set(world, bisonHerdPrefab, ActorSize, {ACTOR_SIZE_LARGE});
}
