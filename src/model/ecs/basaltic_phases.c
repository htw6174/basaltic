#include "basaltic_phases.h"

ECS_TAG_DECLARE(Prep);
ECS_TAG_DECLARE(Execution);
ECS_TAG_DECLARE(Resolution);
ECS_TAG_DECLARE(Cleanup);
ECS_TAG_DECLARE(AdvanceHour);

ECS_TAG_DECLARE(AdvanceDay);
ECS_TAG_DECLARE(AdvanceWeek);
ECS_TAG_DECLARE(AdvanceMonth);
ECS_TAG_DECLARE(AdvanceYear);

ECS_TAG_DECLARE(Planning);

void BcPhasesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcPhases);

    ECS_TAG_DEFINE(world, Prep);
    ECS_TAG_DEFINE(world, Execution);
    ECS_TAG_DEFINE(world, Resolution);
    ECS_TAG_DEFINE(world, Cleanup);
    ECS_TAG_DEFINE(world, AdvanceHour);

    ECS_TAG_DEFINE(world, AdvanceDay);
    ECS_TAG_DEFINE(world, AdvanceWeek);
    ECS_TAG_DEFINE(world, AdvanceMonth);
    ECS_TAG_DEFINE(world, AdvanceYear);

    ECS_TAG_DEFINE(world, Planning);

    ecs_add_id(world, Prep, EcsPhase);
    ecs_add_pair(world, Prep, EcsDependsOn, EcsOnUpdate);
    ecs_add_id(world, Execution, EcsPhase);
    ecs_add_pair(world, Execution, EcsDependsOn, Prep);
    ecs_add_id(world, Resolution, EcsPhase);
    ecs_add_pair(world, Resolution, EcsDependsOn, Execution);
    ecs_add_id(world, Cleanup, EcsPhase);
    ecs_add_pair(world, Cleanup, EcsDependsOn, Resolution);
    ecs_add_id(world, AdvanceHour, EcsPhase);
    ecs_add_pair(world, AdvanceHour, EcsDependsOn, Cleanup);

    ecs_add_id(world, AdvanceDay, EcsPhase);
    ecs_add_id(world, AdvanceWeek, EcsPhase);
    ecs_add_id(world, AdvanceMonth, EcsPhase);
    ecs_add_id(world, AdvanceYear, EcsPhase);

    ecs_add_id(world, Planning, EcsPhase);
    ecs_add_pair(world, Planning, EcsDependsOn, AdvanceHour);
}
