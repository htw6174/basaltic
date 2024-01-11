#ifndef BASALTIC_WORLDGEN_H_INCLUDED
#define BASALTIC_WORLDGEN_H_INCLUDED

#include "htw_core.h"
#include "htw_geomap.h"
#include "components/basaltic_components_planes.h"

void bc_elevationBrush(htw_ChunkMap *chunkMap, htw_geo_GridCoord pos, s32 value, u32 radius);
void bc_wobbleLine(htw_ChunkMap *chunkMap, htw_geo_GridCoord startPos, u32 lineLength, u32 maxValue);

void bc_seedMountains(htw_ChunkMap *chunkMap, u32 mountainRangeCount, u32 rangeSize, u32 maxElevation);
void bc_growMountains(htw_ChunkMap *chunkMap, float slope);

htw_ChunkMap *bc_createTerrain(u32 chunkCountX, u32 chunkCountY);

void bc_generateTerrain(htw_ChunkMap *cm, u32 seed);

/* Rivers */

/// Returns true if any field in waterways is not 0
bool bc_hasAnyWaterways(CellWaterways waterways);

CellWaterConnections bc_extractCellWaterways(const htw_ChunkMap *cm, htw_geo_GridCoord position);

/// Extracts waterway info from cells at a, b, and all of their neighbors, and stores it in an easier to manipulate format
RiverConnection bc_riverConnectionFromCells(const htw_ChunkMap *cm, htw_geo_GridCoord a, htw_geo_GridCoord b);

/// Updates the river segments on both cells uesd to form the connection, and the connection edges between those cells
void bc_applyRiverConnection(htw_ChunkMap *cm, const RiverConnection *rc);

void bc_makeRiverConnection(htw_ChunkMap *cm, htw_geo_GridCoord a, htw_geo_GridCoord b, u8 size);
void bc_removeRiverConnection(htw_ChunkMap *cm, htw_geo_GridCoord a, htw_geo_GridCoord b);

/* Smoothing */
void bc_smoothTerrain(htw_ChunkMap *cm, s32 minProminance);

#endif // BASALTIC_WORLDGEN_H_INCLUDED
