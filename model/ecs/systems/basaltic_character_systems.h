#ifndef BASALTIC_CHARACTER_SYSTEMS_H_INCLUDED
#define BASALTIC_CHARACTER_SYSTEMS_H_INCLUDED

#include "htw_core.h"
#include "htw_geomap.h"
#include "flecs.h"

void bc_createCharacters(ecs_world_t *world, ecs_entity_t terrainMap, size_t count);

// TEST: random movement behavior, pick any adjacent tile to move to
void bc_setWandererDestinations(ecs_iter_t *it);
// TEST: check neighboring cell features, move towards lowest elevation
void bc_setDescenderDestinations(ecs_iter_t *it);
void bc_moveCharacters(ecs_iter_t *it);
void bc_revealMap(ecs_iter_t *it);

void BasalticSystemsCharactersImport(ecs_world_t *world);

#endif // BASALTIC_CHARACTER_SYSTEMS_H_INCLUDED
