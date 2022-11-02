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
    for (int c = 0, y = 0; y < world->chunkCountY; y++) {
        for (int x = 0; x < world->chunkCountX; x++, c++) {
            // generate chunk data
            kd_MapChunk *chunk = &world->chunks[c];
            s32 cellPosX = x * width;
            s32 cellPosY = y * height;
            chunk->heightMap = htw_geo_createValueMap(width, height, 64);
            htw_geo_fillPerlin(chunk->heightMap, 6174, 6, cellPosX, cellPosY, 0.05);

            chunk->temperatureMap = htw_geo_createValueMap(width, height, world->chunkCountY * height);
            htw_geo_fillGradient(chunk->temperatureMap, cellPosY, cellPosY + height);

            chunk->rainfallMap = htw_geo_createValueMap(width, height, 255);
            htw_geo_fillPerlin(chunk->rainfallMap, 8, 2, cellPosX, cellPosY, 0.1);

        }
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
