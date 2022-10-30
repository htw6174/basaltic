#ifndef KINGDOM_WORLDSTATE_H_INCLUDED
#define KINGDOM_WORLDSTATE_H_INCLUDED

#include "htw_geomap.h"

typedef struct kd_MapChunk {
    htw_ValueMap *heightMap;
    htw_ValueMap *temperatureMap;
    htw_ValueMap *rainfallMap;
} kd_MapChunk;

typedef struct kd_worldState {
    u32 chunkCountX, chunkCountY; // number of chunks along each axis
    u32 chunkWidth, chunkHeight; // dimensions of value maps in each chunk
    u32 worldWidth, worldHeight; // total world dimensions
    kd_MapChunk *chunks;
} kd_WorldState;

#endif // KINGDOM_WORLDSTATE_H_INCLUDED
