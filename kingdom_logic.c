#include <math.h>
#include "htw_geomap.h"
#include "kingdom_logic.h"
#include "kingdom_logicInputState.h"
#include "kingdom_worldState.h"

kd_WorldState *kd_createWorldState(u32 chunkCountX, u32 chunkCountY, u32 chunkWidth, u32 chunkHeight) {
    kd_WorldState *newWorld = malloc(sizeof(kd_WorldState));
    newWorld->chunkCountX = chunkCountX;
    newWorld->chunkCountY = chunkCountY;
    newWorld->chunkWidth = chunkWidth;
    newWorld->chunkHeight = chunkHeight;
    newWorld->worldWidth = chunkCountX * chunkWidth;
    newWorld->worldHeight = chunkCountY * chunkHeight;
    return newWorld;
}

int kd_initializeWorldState(kd_WorldState *world) {
    u32 width = world->chunkWidth;
    u32 height = world->chunkHeight;

    u32 chunkCount = world->chunkCountX * world->chunkCountY;
    // TODO: single malloc for all world data, get offset of each array
    world->chunks = malloc(sizeof(kd_MapChunk) * chunkCount);
    for (int c = 0; c < chunkCount; c++) {
        // generate chunk data
        kd_MapChunk *chunk = &world->chunks[c];
        // TODO: use chunk's world position in generation parameters
        chunk->heightMap = htw_geo_createValueMap(width, height, 64);
        htw_geo_fillPerlin(chunk->heightMap, 6174 + c, 6, 0.05);

        chunk->temperatureMap = htw_geo_createValueMap(width, height, 255);
        htw_geo_fillGradient(chunk->temperatureMap, 0, 255);

        chunk->rainfallMap = htw_geo_createValueMap(width, height, 255);
        htw_geo_fillPerlin(chunk->rainfallMap, 8 + c, 2, 0.1);
    }

    return 0;
}

int kd_simulateWorld(kd_LogicInputState *input, kd_WorldState *world) {
    Uint64 ticks = input->ticks;

    if (input->isEditPending) {
        kd_MapEditAction action = input->currentEdit;
        u32 x, y;
        htw_geo_indexToCoords(action.cellIndex, world->chunkWidth, &x, &y);
        u32 chunkIndex = action.chunkIndex;
        s32 currentValue = htw_geo_getMapValue(world->chunks[chunkIndex].heightMap, x, y);
        htw_geo_setMapValue(world->chunks[chunkIndex].heightMap, currentValue + action.value, x, y);
        input->isEditPending = 0;
    }
    return 0;
}

u32 kd_getChunkIndex(kd_WorldState *world, s32 worldX, s32 worldY) {
    s32 chunkX = worldX / world->chunkWidth;
    s32 chunkY = worldY / world->chunkHeight;
    u32 chunkIndex = (chunkY * world->chunkCountX) + chunkX;
    return chunkIndex;
}
