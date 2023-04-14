#include "basaltic_components_actors.h"

ECS_TAG_DECLARE(Ego);

void BasalticComponentsActorsImport(ecs_world_t *world) {

    ECS_MODULE(world, BasalticComponentsActors);

    ECS_TAG_DEFINE(world, Ego);
    ecs_add_id(world, Ego, EcsExclusive);
}
