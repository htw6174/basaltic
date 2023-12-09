#include "bc_flecs_utils.h"
#define BC_COMPONENT_IMPL
#include "bc_components_tribes.h"

void BcTribesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcTribes);

    ECS_META_COMPONENT(world, Tribe);
    ECS_META_COMPONENT(world, Village);
    ECS_META_COMPONENT(world, Stockpile);

    ECS_TAG_DEFINE(world, MemberOf);
    ecs_add_id(world, MemberOf, EcsTraversable);
    ecs_add_id(world, MemberOf, EcsTransitive);

    bc_loadModuleScript(world, "model/plecs/modules");
}
