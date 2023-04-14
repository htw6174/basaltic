#ifndef BASALTIC_TERRAIN_SYSTEMS_H_INCLUDED
#define BASALTIC_TERRAIN_SYSTEMS_H_INCLUDED

#include "flecs.h"

void bc_terrainStep(ecs_iter_t *it);
void bc_terrainSeasonalStep(ecs_iter_t *it);

void bc_terrainCleanEmptyRoots(ecs_iter_t *it);

void BasalticSystemsTerrainImport(ecs_world_t *world);

#endif // BASALTIC_TERRAIN_SYSTEMS_H_INCLUDED
