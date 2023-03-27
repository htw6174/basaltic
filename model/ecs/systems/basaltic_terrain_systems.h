#ifndef BASALTIC_TERRAIN_SYSTEMS_H_INCLUDED
#define BASALTIC_TERRAIN_SYSTEMS_H_INCLUDED

#include "htw_core.h"
#include "htw_geomap.h"
#include "basaltic_worldState.h"
#include "basaltic_terrain.h"
#include "basaltic_components.h"
#include "flecs.h"

ecs_entity_t bc_createTerrain(ecs_world_t *world, u32 chunkCountX, u32 chunkCountY, size_t maxEntities);

void bc_generateTerrain(ecs_world_t *world, ecs_entity_t terrain, u32 seed);

// Allows entities with (GridPosition, (IsOn, _)) to be located via hashmap
void bc_terrainMapAddEntity(ecs_world_t *world, const bc_TerrainMap *tm, ecs_entity_t e, bc_GridPosition pos);
void bc_terrainMapRemoveEntity(ecs_world_t *world, const bc_TerrainMap *tm, ecs_entity_t e, bc_GridPosition pos);
void bc_terrainMapMoveEntity(ecs_world_t *world, const bc_TerrainMap *tm, ecs_entity_t e, bc_GridPosition oldPos, bc_GridPosition newPos);

void bc_terrainStep(ecs_iter_t *it);
void bc_terrainSeasonalStep(ecs_iter_t *it);

void bc_terrainCleanEmptyRoots(ecs_iter_t *it);

void bc_registerTerrainSystems(ecs_world_t *world);

#endif // BASALTIC_TERRAIN_SYSTEMS_H_INCLUDED
