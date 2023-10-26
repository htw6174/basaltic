#ifndef BASALTIC_PHASES_H_INCLUDED
#define BASALTIC_PHASES_H_INCLUDED

#include "flecs.h"

#undef ECS_META_IMPL
#undef BC_DECL
#ifndef BC_COMPONENT_IMPL
#define ECS_META_IMPL EXTERN
#define BC_DECL extern
#else
#define BC_DECL
#endif

BC_DECL ECS_TAG_DECLARE(Prep);
BC_DECL ECS_TAG_DECLARE(Execution);
BC_DECL ECS_TAG_DECLARE(Resolution);
BC_DECL ECS_TAG_DECLARE(Cleanup);
BC_DECL ECS_TAG_DECLARE(AdvanceStep);
// AI actors plan their next move before step ends
BC_DECL ECS_TAG_DECLARE(Planning);

BC_DECL ECS_ENTITY_DECLARE(TickHour);
BC_DECL ECS_ENTITY_DECLARE(TickDay);
BC_DECL ECS_ENTITY_DECLARE(TickWeek);
BC_DECL ECS_ENTITY_DECLARE(TickMonth);
BC_DECL ECS_ENTITY_DECLARE(TickYear);

void BcPhasesImport(ecs_world_t *world);

#endif // BASALTIC_PHASES_H_INCLUDED
