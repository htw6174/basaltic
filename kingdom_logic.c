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

    world->heightMap = htw_geo_createValueMap(width, height, 64);
    htw_geo_fillPerlin(world->heightMap, 6174, 6, 0.05);

    world->temperatureMap = htw_geo_createValueMap(width, height, 255);
    htw_geo_fillGradient(world->temperatureMap, 0, 255);

    world->rainfallMap = htw_geo_createValueMap(width, height, 255);
    htw_geo_fillPerlin(world->rainfallMap, 8, 2, 0.1);
    return 0;
}

int kd_simulateWorld(kd_LogicInputState *input, kd_WorldState *world) {
    Uint64 ticks = input->ticks;

    if (input->isEditPending) {
        kd_MapEditAction action = input->currentEdit;
        u32 x, y;
        htw_geo_indexToCoords(action.cellIndex, world->heightMap->width, &x, &y);
        s32 currentValue = htw_geo_getMapValue(world->heightMap, x, y);
        htw_geo_setMapValue(world->heightMap, currentValue + action.value, x, y);
        input->isEditPending = 0;
    }
    //world->heightMap->maxMagnitude = (int)(((sin((double)ticks / 1000) + 1) / 2) * 255);
    //htw_geo_fillGradient(world->heightMap, 0, world->heightMap->maxMagnitude);
    return 0;
}
