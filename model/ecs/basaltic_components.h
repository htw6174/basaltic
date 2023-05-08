#ifndef BASALTIC_COMPONENTS_H_INCLUDED
#define BASALTIC_COMPONENTS_H_INCLUDED

#include "flecs.h"
#include "components/basaltic_components_planes.h"
#include "components/basaltic_components_actors.h"

// TODO: find a home for this
extern ECS_TAG_DECLARE(PlayerVision);

void BasalticComponentsImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_H_INCLUDED
