#ifndef BASALTIC_PHASES_VIEW_H_INCLUDED
#define BASALTIC_PHASES_VIEW_H_INCLUDED

#include "flecs.h"

extern ECS_TAG_DECLARE(OnModelChanged);

extern ECS_DECLARE(ModelChangedPipeline);

void BasalticPhasesViewImport(ecs_world_t *world);

#endif // BASALTIC_PHASES_VIEW_H_INCLUDED
