#ifndef BASALTIC_WORLDGEN_H_INCLUDED
#define BASALTIC_WORLDGEN_H_INCLUDED

#include "htw_core.h"
#include "htw_geomap.h"
#include "basaltic_worldState.h"

void bc_elevationBrush(htw_ChunkMap *chunkMap, htw_geo_GridCoord pos, s32 value, u32 radius);
void bc_wobbleLine(htw_ChunkMap *chunkMap, htw_geo_GridCoord startPos, u32 lineLength, u32 maxValue);

void bc_seedMountains(htw_ChunkMap *chunkMap, u32 mountainRangeCount, u32 rangeSize, u32 maxElevation);
void bc_growMountains(htw_ChunkMap *chunkMap, float slope);

#endif // BASALTIC_WORLDGEN_H_INCLUDED
