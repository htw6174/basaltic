#ifndef KINGDOM_WORLDSTATE_H_INCLUDED
#define KINGDOM_WORLDSTATE_H_INCLUDED

#include "htw_geomap.h"
#include "kingdom_characters.h"

typedef struct {
    htw_ValueMap *heightMap;
    htw_ValueMap *temperatureMap;
    htw_ValueMap *rainfallMap;
    htw_ValueMap *visibilityMap;
} kd_MapChunk;

typedef struct {
    // Geography
    u32 chunkCountX, chunkCountY; // number of chunks along each axis
    u32 chunkWidth, chunkHeight; // dimensions of value maps in each chunk
    u32 worldWidth, worldHeight; // total world dimensions
    kd_MapChunk *chunks;
    // Characters
    u32 characterPoolSize;
    kd_Character *characters;
} kd_WorldState;

static u32 kd_getChunkIndexByChunkCoordinates(kd_WorldState *world, htw_geo_GridCoord chunkCoord);
static u32 kd_getChunkIndexByWorldCoordinates(kd_WorldState *world, htw_geo_GridCoord worldCoord);
static u32 kd_getChunkIndexByWorldPosition(kd_WorldState *world, float worldX, float worldY);
static u32 kd_getChunkIndexAtOffset(kd_WorldState *world, u32 startingChunk, htw_geo_GridCoord chunkOffset);
static void kd_gridCoordinatesToChunkAndCell(kd_WorldState *world, htw_geo_GridCoord worldCoord, u32 *chunkIndex, u32 *cellIndex);
static htw_geo_GridCoord kd_chunkAndCellToWorldCoordinates(kd_WorldState *world, u32 chunkIndex, u32 cellIndex);
static void kd_getChunkRootPosition(kd_WorldState *world, u32 chunkIndex, float *worldX, float *worldY);

static u32 kd_getChunkIndexByChunkCoordinates(kd_WorldState *world, htw_geo_GridCoord chunkCoord) {
    // horizontal wrap
    chunkCoord.x = world->chunkCountX + chunkCoord.x; // to account for negative chunkX
    chunkCoord.x = chunkCoord.x % world->chunkCountX;
    // vertical clamp
    chunkCoord.y = max_int(0, min_int(world->chunkCountY - 1, chunkCoord.y));
    u32 chunkIndex = (chunkCoord.y * world->chunkCountX) + chunkCoord.x;
    return chunkIndex;
}

static u32 kd_getChunkIndexByWorldCoordinates(kd_WorldState *world, htw_geo_GridCoord worldCoord) {
    htw_geo_GridCoord chunkCoord = {
        .x = worldCoord.x / world->chunkWidth,
        .y = worldCoord.y / world->chunkHeight
    };
    return kd_getChunkIndexByChunkCoordinates(world, chunkCoord);
}

static u32 kd_getChunkIndexByWorldPosition(kd_WorldState *world, float worldX, float worldY) {
    // reverse hex grid coordinate skewing
    float deskewedY = (1.0 / sqrt(0.75)) * worldY;
    float deskewedX = worldX - (deskewedY * 0.5);
    // convert to chunk grid coordinates
    htw_geo_GridCoord chunkCoord = {
        .x = floorf(deskewedX / world->chunkWidth),
        .y = floorf(deskewedY / world->chunkHeight)
    };
    return kd_getChunkIndexByChunkCoordinates(world, chunkCoord);
}

static u32 kd_getChunkIndexAtOffset(kd_WorldState *world, u32 startingChunk, htw_geo_GridCoord chunkOffset) {
    htw_geo_GridCoord chunkCoord = {
        .x = startingChunk % world->chunkCountX,
        .y = startingChunk / world->chunkCountX
    };
    chunkCoord = htw_geo_addGridCoords(chunkCoord, chunkOffset);
    return kd_getChunkIndexByChunkCoordinates(world, chunkCoord);
}

static void kd_gridCoordinatesToChunkAndCell(kd_WorldState *world, htw_geo_GridCoord worldCoord, u32 *chunkIndex, u32 *cellIndex) {
    *chunkIndex = kd_getChunkIndexByWorldCoordinates(world, worldCoord);
    htw_geo_GridCoord chunkCoord = {
        .x = *chunkIndex % world->chunkCountX,
        .y = *chunkIndex / world->chunkCountX
    };
    htw_geo_GridCoord cellCoord = {
        .x = worldCoord.x - (chunkCoord.x * world->chunkWidth),
        .y = worldCoord.y - (chunkCoord.y * world->chunkHeight)
    };
    *cellIndex = cellCoord.x + (cellCoord.y * world->chunkWidth);
}

static htw_geo_GridCoord kd_chunkAndCellToWorldCoordinates(kd_WorldState *world, u32 chunkIndex, u32 cellIndex) {
    u32 chunkX = chunkIndex % world->chunkCountX;
    u32 chunkY = chunkIndex / world->chunkCountX;
    u32 cellX = cellIndex % world->chunkWidth;
    u32 cellY = cellIndex / world->chunkWidth;
    htw_geo_GridCoord worldCoord = {
        .x = (chunkX * world->chunkWidth) + cellX,
        .y = (chunkY * world->chunkHeight) + cellY
    };
    return worldCoord;
}

static void kd_getChunkRootPosition(kd_WorldState *world, u32 chunkIndex, float *worldX, float *worldY) {
    u32 chunkX = chunkIndex % world->chunkCountX;
    u32 chunkY = chunkIndex / world->chunkCountY;
    s32 gridX = chunkX * world->chunkWidth;
    s32 gridY = chunkY * world->chunkHeight;
    htw_geo_getHexCellPositionSkewed((htw_geo_GridCoord){gridX, gridY}, worldX, worldY);
}

#endif // KINGDOM_WORLDSTATE_H_INCLUDED
