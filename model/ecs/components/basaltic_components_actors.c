#include "basaltic_components_actors.h"

ECS_TAG_DECLARE(Ego);

void BcActorsImport(ecs_world_t *world) {

    ECS_MODULE(world, BcActors);

    ECS_TAG_DEFINE(world, Ego);
    ecs_add_id(world, Ego, EcsExclusive);
}
