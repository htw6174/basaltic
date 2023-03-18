#ifndef BASALTIC_WORLDSTATE_H_INCLUDED
#define BASALTIC_WORLDSTATE_H_INCLUDED

#include <math.h>
#include "htw_geomap.h"
#include "basaltic_characters.h"
#include "flecs.h"

#define BC_MAX_SEED_LENGTH 256

static const u32 bc_chunkSize = 64;

typedef struct {
    s32 height;
    s32 temperature;
    s32 nutrient;
    s32 rainfall;
    u32 visibility;
    s32 vegetation;
} bc_CellData;

typedef struct {
    u32 seed;
    char *seedString;
    u64 step;
    // Geography
    htw_ChunkMap *surfaceMap;
    // Characters
    ecs_world_t *ecsWorld;
    //bc_CharacterPool *characterPool;
} bc_WorldState;

static bc_CellData *bc_getCellByIndex(htw_ChunkMap *chunkMap, u32 chunkIndex, u32 cellIndex);

static u32 bc_getChunkIndexByWorldPosition(bc_WorldState *world, float worldX, float worldY);


static bc_CellData *bc_getCellByIndex(htw_ChunkMap *chunkMap, u32 chunkIndex, u32 cellIndex) {
    bc_CellData *cell = chunkMap->chunks[chunkIndex].cellData;
    return &cell[cellIndex];
}

// TODO: move this to htw_geomap
static u32 bc_getChunkIndexByWorldPosition(bc_WorldState *world, float worldX, float worldY) {
    // reverse hex grid coordinate skewing
    float deskewedY = (1.0 / sqrt(0.75)) * worldY;
    float deskewedX = worldX - (deskewedY * 0.5);
    // convert to chunk grid coordinates
    htw_geo_GridCoord chunkCoord = {
        .x = floorf(deskewedX / bc_chunkSize),
        .y = floorf(deskewedY / bc_chunkSize)
    };
    return htw_geo_getChunkIndexByChunkCoordinates(world->surfaceMap, chunkCoord);
}

#endif // BASALTIC_WORLDSTATE_H_INCLUDED
