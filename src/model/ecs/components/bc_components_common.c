#include <sys/stat.h>

#define BC_COMPONENT_IMPL
#include "bc_components_common.h"

void BcCommonImport(ecs_world_t *world) {
    ECS_MODULE(world, BcCommon);

    // Creating meta info for custom primitive types:
    // 1. Declare and Define a component for the type
    // 2. Use ecs_primitive to create meta info
    ECS_COMPONENT_DEFINE(world, s8);
    ECS_COMPONENT_DEFINE(world, s16);
    ECS_COMPONENT_DEFINE(world, s32);
    ECS_COMPONENT_DEFINE(world, s64);

    ECS_COMPONENT_DEFINE(world, Step);
    ECS_COMPONENT_DEFINE(world, time_t);

    ecs_primitive(world, {.entity = ecs_id(s8), .kind = EcsI8});
    ecs_primitive(world, {.entity = ecs_id(s16), .kind = EcsI16});
    ecs_primitive(world, {.entity = ecs_id(s32), .kind = EcsI32});
    ecs_primitive(world, {.entity = ecs_id(s64), .kind = EcsI64});

    ecs_primitive(world, {.entity = ecs_id(Step), .kind = EcsU64});
    ecs_primitive(world, {.entity = ecs_id(time_t), .kind = EcsU64});


    ECS_META_COMPONENT(world, RandomizerDistribution);
    ECS_META_COMPONENT(world, RandomizeInt);

    ECS_META_COMPONENT(world, ResourceFile);

    ECS_TAG_DEFINE(world, FlecsScriptSource);
}
