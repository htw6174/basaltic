#define BC_COMPONENT_IMPL
#include "basaltic_phases.h"

void BcPhasesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcPhases);

    ECS_TAG_DEFINE(world, Prep);
    ECS_TAG_DEFINE(world, Execution);
    ECS_TAG_DEFINE(world, Resolution);
    ECS_TAG_DEFINE(world, Cleanup);
    ECS_TAG_DEFINE(world, AdvanceStep);

    ECS_ENTITY_DEFINE(world, TickHour);
    ECS_ENTITY_DEFINE(world, TickDay);
    ECS_ENTITY_DEFINE(world, TickWeek);
    ECS_ENTITY_DEFINE(world, TickMonth);
    ECS_ENTITY_DEFINE(world, TickYear);

    ECS_TAG_DEFINE(world, Planning);

    ecs_add_id(world, Prep, EcsPhase);
    ecs_add_pair(world, Prep, EcsDependsOn, EcsOnUpdate);
    ecs_add_id(world, Execution, EcsPhase);
    ecs_add_pair(world, Execution, EcsDependsOn, Prep);
    ecs_add_id(world, Resolution, EcsPhase);
    ecs_add_pair(world, Resolution, EcsDependsOn, Execution);
    ecs_add_id(world, Cleanup, EcsPhase);
    ecs_add_pair(world, Cleanup, EcsDependsOn, Resolution);
    ecs_add_id(world, AdvanceStep, EcsPhase);
    ecs_add_pair(world, AdvanceStep, EcsDependsOn, Cleanup);
    ecs_add_id(world, Planning, EcsPhase);
    ecs_add_pair(world, Planning, EcsDependsOn, AdvanceStep);

    // Setup tick sources
    ecs_set_rate(world, TickHour, 1, 0);
    ecs_set_rate(world, TickDay, 24, TickHour);
    ecs_set_rate(world, TickWeek, 7, TickDay);
    ecs_set_rate(world, TickMonth, 4, TickWeek);
    ecs_set_rate(world, TickYear, 12, TickMonth);
}
