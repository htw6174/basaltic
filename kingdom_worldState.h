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

static u32 kd_getChunkIndexByChunkCoordinates(kd_WorldState *world, s32 chunkX, s32 chunkY);
static u32 kd_getChunkIndexByGridCoordinates(kd_WorldState *world, s32 gridX, s32 gridY);
static u32 kd_getChunkIndexByWorldPosition(kd_WorldState *world, float worldX, float worldY);
static u32 kd_getChunkIndexAtOffset(kd_WorldState *world, u32 startingChunk, s32 offsetX, s32 offsetY);
static void kd_gridCoordinatesToChunkAndCell(kd_WorldState *world, s32 gridX, s32 gridY, u32 *chunkIndex, u32 *cellIndex);
static void kd_chunkAndCellToGridCoordinates(kd_WorldState *world, u32 chunkIndex, u32 cellIndex, u32 *gridX, u32 *gridY);
static void kd_getChunkRootPosition(kd_WorldState *world, u32 chunkIndex, float *worldX, float *worldY);

static u32 kd_getChunkIndexByChunkCoordinates(kd_WorldState *world, s32 chunkX, s32 chunkY) {
    // horizontal wrap
    chunkX = world->chunkCountX + chunkX; // to account for negative chunkX
    chunkX = chunkX % world->chunkCountX;
    // vertical clamp
    chunkY = max_int(0, min_int(world->chunkCountY - 1, chunkY));
    u32 chunkIndex = (chunkY * world->chunkCountX) + chunkX;
    return chunkIndex;
}

static u32 kd_getChunkIndexByGridCoordinates(kd_WorldState *world, s32 gridX, s32 gridY) {
    s32 chunkX = gridX / world->chunkWidth;
    s32 chunkY = gridY / world->chunkHeight;
    return kd_getChunkIndexByChunkCoordinates(world, chunkX, chunkY);
}

static u32 kd_getChunkIndexByWorldPosition(kd_WorldState *world, float worldX, float worldY) {
    // reverse hex grid coordinate skewing
    float deskewedY = (1.0 / sqrt(0.75)) * worldY;
    float deskewedX = worldX - (deskewedY * 0.5);
    // convert to chunk grid coordinates
    s32 chunkX = floorf(deskewedX / world->chunkWidth);
    s32 chunkY = floorf(deskewedY / world->chunkHeight);
    return kd_getChunkIndexByChunkCoordinates(world, chunkX, chunkY);
}

static u32 kd_getChunkIndexAtOffset(kd_WorldState *world, u32 startingChunk, s32 offsetX, s32 offsetY) {
    s32 chunkX = startingChunk % world->chunkCountX;
    s32 chunkY = startingChunk / world->chunkCountX;
    chunkX += offsetX;
    chunkY += offsetY;
    return kd_getChunkIndexByChunkCoordinates(world, chunkX, chunkY);
}

static void kd_gridCoordinatesToChunkAndCell(kd_WorldState *world, s32 gridX, s32 gridY, u32 *chunkIndex, u32 *cellIndex) {
    *chunkIndex = kd_getChunkIndexByGridCoordinates(world, gridX, gridY);
    u32 chunkX = *chunkIndex % world->chunkCountX;
    u32 chunkY = *chunkIndex / world->chunkCountX;
    u32 cellX = gridX - (chunkX * world->chunkWidth);
    u32 cellY = gridY - (chunkY * world->chunkHeight);
    *cellIndex = cellX + (cellY * world->chunkWidth);
}

static void kd_chunkAndCellToGridCoordinates(kd_WorldState *world, u32 chunkIndex, u32 cellIndex, u32 *gridX, u32 *gridY) {
    u32 chunkX = chunkIndex % world->chunkCountX;
    u32 chunkY = chunkIndex / world->chunkCountX;
    u32 cellX = cellIndex % world->chunkWidth;
    u32 cellY = cellIndex / world->chunkWidth;
    *gridX = (chunkX * world->chunkWidth) + cellX;
    *gridY = (chunkY * world->chunkHeight) + cellY;
}

static void kd_getChunkRootPosition(kd_WorldState *world, u32 chunkIndex, float *worldX, float *worldY) {
    u32 chunkX = chunkIndex % world->chunkCountX;
    u32 chunkY = chunkIndex / world->chunkCountY;
    s32 gridX = chunkX * world->chunkWidth;
    s32 gridY = chunkY * world->chunkHeight;
    htw_geo_getHexCellPositionSkewed((htw_geo_GridCoord){gridX, gridY}, worldX, worldY);
}

#endif // KINGDOM_WORLDSTATE_H_INCLUDED
