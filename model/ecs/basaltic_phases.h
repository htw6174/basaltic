#ifndef BASALTIC_PHASES_H_INCLUDED
#define BASALTIC_PHASES_H_INCLUDED

#include "flecs.h"

extern ECS_TAG_DECLARE(Planning);
extern ECS_TAG_DECLARE(Prep);
extern ECS_TAG_DECLARE(Execution);
extern ECS_TAG_DECLARE(Resolution);
extern ECS_TAG_DECLARE(Cleanup);
extern ECS_TAG_DECLARE(AdvanceHour);
extern ECS_TAG_DECLARE(AdvanceDay);
extern ECS_TAG_DECLARE(AdvanceWeek);
extern ECS_TAG_DECLARE(AdvanceMonth);
extern ECS_TAG_DECLARE(AdvanceYear);

void BcPhasesImport(ecs_world_t *world);

#endif // BASALTIC_PHASES_H_INCLUDED
