#define BASALTIC_ACTORS_IMPL
#include "basaltic_components_actors.h"

ECS_TAG_DECLARE(Ego);
ECS_TAG_DECLARE(EgoNone);
ECS_TAG_DECLARE(EgoWanderer);

ECS_TAG_DECLARE(FollowerOf);

ECS_TAG_DECLARE(Individual);

void BcActorsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcActors);

    ECS_TAG_DEFINE(world, Ego);
    ecs_add_id(world, Ego, EcsExclusive);
    ecs_add_id(world, Ego, EcsOneOf);

    ECS_TAG_DEFINE(world, EgoNone);
    ecs_add_pair(world, EgoNone, EcsChildOf, Ego);
    ECS_TAG_DEFINE(world, EgoWanderer);
    ecs_add_pair(world, EgoWanderer, EcsChildOf, Ego);

    ECS_TAG_DEFINE(world, FollowerOf);
    ecs_add_id(world, FollowerOf, EcsExclusive);

    ECS_TAG_DEFINE(world, Individual);

    ECS_META_COMPONENT(world, Group);

    ECS_META_COMPONENT(world, ActorSize);
}
