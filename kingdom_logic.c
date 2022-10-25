#include <math.h>
#include "htw_geomap.h"
#include "kingdom_logic.h"
#include "kingdom_logicInputState.h"
#include "kingdom_worldState.h"

kd_WorldState *kd_createWorldState(char *name) {
    kd_WorldState *newWorld = malloc(sizeof(kd_WorldState));
    newWorld->name = name; // Need to copy first?
    return newWorld;
}

int kd_initializeWorldState(kd_WorldState *world, unsigned int width, unsigned int height) {
    world->heightMap = htw_geo_createValueMap(width, height, 255);
    htw_geo_fillGradient(world->heightMap, 0, 32);
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
