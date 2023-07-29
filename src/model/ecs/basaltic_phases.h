#ifndef BASALTIC_PHASES_H_INCLUDED
#define BASALTIC_PHASES_H_INCLUDED

#include "flecs.h"

extern ECS_TAG_DECLARE(Prep);
extern ECS_TAG_DECLARE(Execution);
extern ECS_TAG_DECLARE(Resolution);
extern ECS_TAG_DECLARE(Cleanup);
extern ECS_TAG_DECLARE(AdvanceHour);

// Not part of the normal pipeline, these phases are run standalone when needed, in order after AdvanceHour
extern ECS_TAG_DECLARE(AdvanceDay);
extern ECS_TAG_DECLARE(AdvanceWeek);
extern ECS_TAG_DECLARE(AdvanceMonth);
extern ECS_TAG_DECLARE(AdvanceYear);

extern ECS_TAG_DECLARE(Planning);
// New step begins after AI actors plan their next move

void BcPhasesImport(ecs_world_t *world);

#endif // BASALTIC_PHASES_H_INCLUDED
