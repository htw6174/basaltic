#include <math.h>
#include "kingdom_logic.h"

kd_WorldState *kd_createWorldState(char *name) {
    kd_WorldState *newWorld = malloc(sizeof(kd_WorldState));
    newWorld->name = name; // Need to copy first?
    return newWorld;
}

int kd_initializeWorldState(kd_WorldState *world, unsigned int width, unsigned int height) {
    world->heightMap = createValueMap(width, height, 255);
    fillGradient(world->heightMap, 0, 255);
    return 0;
}

int kd_simulateWorld(kd_LogicInputState *input, kd_WorldState *world) {
    Uint64 ticks = input->ticks;
    world->heightMap->maxValue = (int)(((sin((double)ticks / 1000) + 1) / 2) * 255);
    fillGradient(world->heightMap, 0, world->heightMap->maxValue);
    return 0;
}
