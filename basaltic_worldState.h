#ifndef BASALTIC_WORLDSTATE_H_INCLUDED
#define BASALTIC_WORLDSTATE_H_INCLUDED

#include <math.h>
#include "htw_geomap.h"
#include "basaltic_commandQueue.h"
#include "basaltic_characters.h"

#define BC_MAX_SEED_LENGTH 256

typedef struct {
    htw_ValueMap *heightMap;
    htw_ValueMap *temperatureMap;
    htw_ValueMap *rainfallMap;
    htw_ValueMap *visibilityMap;
} bc_MapChunk;

typedef struct {
    u32 seed;
    char *seedString;
    u64 step;
    // Geography
    u32 chunkCountX, chunkCountY; // number of chunks along each axis
    u32 worldWidth, worldHeight; // total world dimensions
    bc_MapChunk *chunks;
    // Characters
    u32 characterPoolSize;
    bc_Character *characters;
} bc_WorldState;

static u32 bc_getChunkIndexByChunkCoordinates(bc_WorldState *world, htw_geo_GridCoord chunkCoord);
static u32 bc_getChunkIndexByWorldCoordinates(bc_WorldState *world, htw_geo_GridCoord worldCoord);
static u32 bc_getChunkIndexByWorldPosition(bc_WorldState *world, float worldX, float worldY);
static u32 bc_getChunkIndexAtOffset(bc_WorldState *world, u32 startingChunk, htw_geo_GridCoord chunkOffset);
static void bc_gridCoordinatesToChunkAndCell(bc_WorldState *world, htw_geo_GridCoord worldCoord, u32 *chunkIndex, u32 *cellIndex);
static htw_geo_GridCoord bc_chunkAndCellToWorldCoordinates(bc_WorldState *world, u32 chunkIndex, u32 cellIndex);
static void bc_getChunkRootPosition(bc_WorldState *world, u32 chunkIndex, float *worldX, float *worldY);

static u32 bc_getChunkIndexByChunkCoordinates(bc_WorldState *world, htw_geo_GridCoord chunkCoord) {
    // horizontal wrap
    chunkCoord.x = world->chunkCountX + chunkCoord.x; // to account for negative chunkX
    chunkCoord.x = chunkCoord.x % world->chunkCountX;
    // vertical wrap
    chunkCoord.y += world->chunkCountY;
    chunkCoord.y %= world->chunkCountY;
    u32 chunkIndex = (chunkCoord.y * world->chunkCountX) + chunkCoord.x;
    return chunkIndex;
}

static u32 bc_getChunkIndexByWorldCoordinates(bc_WorldState *world, htw_geo_GridCoord worldCoord) {
    htw_geo_GridCoord chunkCoord = {
        .x = worldCoord.x / bc_chunkSize,
        .y = worldCoord.y / bc_chunkSize
    };
    return bc_getChunkIndexByChunkCoordinates(world, chunkCoord);
}

static u32 bc_getChunkIndexByWorldPosition(bc_WorldState *world, float worldX, float worldY) {
    // reverse hex grid coordinate skewing
    float deskewedY = (1.0 / sqrt(0.75)) * worldY;
    float deskewedX = worldX - (deskewedY * 0.5);
    // convert to chunk grid coordinates
    htw_geo_GridCoord chunkCoord = {
        .x = floorf(deskewedX / bc_chunkSize),
        .y = floorf(deskewedY / bc_chunkSize)
    };
    return bc_getChunkIndexByChunkCoordinates(world, chunkCoord);
}

static u32 bc_getChunkIndexAtOffset(bc_WorldState *world, u32 startingChunk, htw_geo_GridCoord chunkOffset) {
    htw_geo_GridCoord chunkCoord = {
        .x = startingChunk % world->chunkCountX,
        .y = startingChunk / world->chunkCountX
    };
    chunkCoord = htw_geo_addGridCoords(chunkCoord, chunkOffset);
    return bc_getChunkIndexByChunkCoordinates(world, chunkCoord);
}

static void bc_gridCoordinatesToChunkAndCell(bc_WorldState *world, htw_geo_GridCoord worldCoord, u32 *chunkIndex, u32 *cellIndex) {
    worldCoord.x += world->worldWidth;
    worldCoord.x %= world->worldWidth;
    worldCoord.y += world->worldHeight;
    worldCoord.y %= world->worldHeight;

    *chunkIndex = bc_getChunkIndexByWorldCoordinates(world, worldCoord);
    htw_geo_GridCoord chunkCoord = {
        .x = *chunkIndex % world->chunkCountX,
        .y = *chunkIndex / world->chunkCountX
    };
    htw_geo_GridCoord cellCoord = {
        .x = worldCoord.x - (chunkCoord.x * bc_chunkSize),
        .y = worldCoord.y - (chunkCoord.y * bc_chunkSize)
    };
    *cellIndex = cellCoord.x + (cellCoord.y * bc_chunkSize);
}

static htw_geo_GridCoord bc_chunkAndCellToWorldCoordinates(bc_WorldState *world, u32 chunkIndex, u32 cellIndex) {
    u32 chunkX = chunkIndex % world->chunkCountX;
    u32 chunkY = chunkIndex / world->chunkCountX;
    u32 cellX = cellIndex % bc_chunkSize;
    u32 cellY = cellIndex / bc_chunkSize;
    htw_geo_GridCoord worldCoord = {
        .x = (chunkX * bc_chunkSize) + cellX,
        .y = (chunkY * bc_chunkSize) + cellY
    };
    return worldCoord;
}

static void bc_getChunkRootPosition(bc_WorldState *world, u32 chunkIndex, float *worldX, float *worldY) {
    u32 chunkX = chunkIndex % world->chunkCountX;
    u32 chunkY = chunkIndex / world->chunkCountY;
    s32 gridX = chunkX * bc_chunkSize;
    s32 gridY = chunkY * bc_chunkSize;
    htw_geo_getHexCellPositionSkewed((htw_geo_GridCoord){gridX, gridY}, worldX, worldY);
}

#endif // BASALTIC_WORLDSTATE_H_INCLUDED
