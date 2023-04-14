#ifndef BASALTIC_COMPONENTS_ACTORS_H_INCLUDED
#define BASALTIC_COMPONENTS_ACTORS_H_INCLUDED

#include "flecs.h"

extern ECS_TAG_DECLARE(Ego); // Relationship type paired with a 'controller': either an AI system or a player

void BasalticComponentsActorsImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_ACTORS_H_INCLUDED
