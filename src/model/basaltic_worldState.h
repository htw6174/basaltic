#ifndef BASALTIC_WORLDSTATE_H_INCLUDED
#define BASALTIC_WORLDSTATE_H_INCLUDED

#include <math.h>
#include <SDL2/SDL_mutex.h>
#include "htw_geomap.h"
#include "flecs.h"

#define BC_MAX_SEED_LENGTH 256

static const u32 bc_chunkSize = 64;

typedef enum bc_TerrainVisibilityBitFlags {
    BC_TERRAIN_VISIBILITY_GEOMETRY =    0x00000001, // outline of cells and their topography (heightmap value)
    BC_TERRAIN_VISIBILITY_COLOR =       0x00000002, // base color from biome
    BC_TERRAIN_VISIBILITY_MEGALITHS =   0x00000004, // large ruins, cities, and castles. Usually visible whenever geometry is
    BC_TERRAIN_VISIBILITY_VEGETATION =  0x00000008, // presence or absence of trees and shrubs. Usually visible whenever color is
    BC_TERRAIN_VISIBILITY_STRUCTURES =  0x00000010, // roads, towns, and farmhouses in plain view
    BC_TERRAIN_VISIBILITY_MEGAFAUNA =   0x00000020, // huge size creatures or large herds, usually visible from a distance
    BC_TERRAIN_VISIBILITY_CHARACTERS =  0x00000040, // doesn't reveal specifics, just signs of life and activity (e.g. campfire smoke)
    BC_TERRAIN_VISIBILITY_SECRETS =     0x00000080, // hidden cave entrances, lairs, or isolated buildings
    BC_TERRAIN_VISIBILITY_ALL =         0x000000ff,
} bc_TerrainVisibilityBitFlags;

typedef struct {
    u32 seed;
    char *seedString;
    u64 step;
    // ECS
    ecs_world_t *ecsWorld;
    ecs_world_t *readonlyWorld;
    ecs_query_t *systems;
    // Geography
    ecs_entity_t centralPlane;
    ecs_query_t *planes;
    // Characters
    ecs_query_t *characters;
    // world access lock
    SDL_sem *lock;
} bc_WorldState;


static u32 bc_getChunkIndexByWorldPosition(htw_ChunkMap *chunkMap, float worldX, float worldY);

// TODO: move this to htw_geomap?
static u32 bc_getChunkIndexByWorldPosition(htw_ChunkMap *chunkMap, float worldX, float worldY) {
    // reverse hex grid coordinate skewing
    float deskewedY = (1.0 / sqrt(0.75)) * worldY;
    float deskewedX = worldX - (deskewedY * 0.5);
    // convert to chunk grid coordinates
    htw_geo_GridCoord chunkCoord = {
        .x = floorf(deskewedX / bc_chunkSize),
        .y = floorf(deskewedY / bc_chunkSize)
    };
    return htw_geo_getChunkIndexByChunkCoordinates(chunkMap, chunkCoord);
}

#endif // BASALTIC_WORLDSTATE_H_INCLUDED
