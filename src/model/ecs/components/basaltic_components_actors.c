#include "bc_flecs_utils.h"
#define BC_COMPONENT_IMPL
#include "basaltic_components_actors.h"

void BcActorsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcActors);

    /* ego: AI behavior types */
    ECS_TAG_DEFINE(world, Ego);
    ecs_add_id(world, Ego, EcsExclusive);
    ecs_add_id(world, Ego, EcsOneOf);

    ECS_TAG_DEFINE(world, EgoNone);
    ecs_add_pair(world, EgoNone, EcsChildOf, Ego);
    ECS_TAG_DEFINE(world, EgoWanderer);
    ecs_add_pair(world, EgoWanderer, EcsChildOf, Ego);

    /* action: selected by an ego */
    ECS_TAG_DEFINE(world, Action);
    ecs_add_id(world, Action, EcsUnion);
    ecs_add_id(world, Action, EcsOneOf);

    ECS_TAG_DEFINE(world, ActionIdle);
    ecs_add_pair(world, ActionIdle, EcsChildOf, Action);
    ECS_TAG_DEFINE(world, ActionMove);
    ecs_add_pair(world, ActionMove, EcsChildOf, Action);

    /* other */
    ECS_TAG_DEFINE(world, FollowerOf);
    ecs_add_id(world, FollowerOf, EcsExclusive);

    ECS_TAG_DEFINE(world, Individual);
    ECS_META_COMPONENT(world, Group);

    ECS_META_COMPONENT(world, MapVision);

    ECS_META_COMPONENT(world, GrowthRate);


    ECS_COMPONENT_DEFINE(world, CreationTime);
    ecs_primitive(world, {.entity = ecs_id(CreationTime), .kind = EcsU64});

    ECS_META_COMPONENT(world, Spawner);

    ECS_META_COMPONENT(world, ActorSize);

    ECS_META_COMPONENT(world, Condition);

    ECS_META_COMPONENT(world, Stats);
    ECS_META_COMPONENT(world, AbilityModifiers);

    ECS_META_COMPONENT(world, Skill);
    ecs_add_id(world, ecs_id(Skill), EcsOneOf);

    bc_loadModuleScript(world, "model/plecs/modules");
}
