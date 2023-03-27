#ifndef BASALTIC_TERRAIN_H_INCLUDED
#define BASALTIC_TERRAIN_H_INCLUDED

#include "flecs.h"
#include "htw_geomap.h"
#include "htw_random.h"
#include "khash.h"

#define bc_gridCoord_hash_func(key) xxh_hash2d(0, key.x, key.y)
#define bc_gridCoord_hash_equal(a, b) htw_geo_isEqualGridCoords(a, b)

KHASH_INIT(GridMap, htw_geo_GridCoord, ecs_entity_t, 1, bc_gridCoord_hash_func, bc_gridCoord_hash_equal)

typedef struct {
    htw_ChunkMap *chunkMap;
    khash_t(GridMap) *gridMap;
} bc_TerrainMap;

#endif // BASALTIC_TERRAIN_H_INCLUDED
