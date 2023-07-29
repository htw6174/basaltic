/* TODO: consider removing void* cell data in favor of macro-based custom types; direct access to cell data would be slightly faster and more convenient, but passing around custom chunkmap references would be less convenient
 * Advantage of not using custom type macros: source files working with chunkmaps don't need to know what kind of data they contain, if all it cares about is relative position or passing a celldata reference to something else
 */
#include <math.h>
#include "htw_geomap.h"
#include "htw_core.h"

htw_ChunkMap *htw_geo_createChunkMap(u32 chunkSize, u32 chunkCountX, u32 chunkCountY, size_t cellDataSize) {
    htw_ChunkMap *newWorldMap = calloc(1, sizeof(htw_ChunkMap));
    newWorldMap->chunkSize = chunkSize;
    newWorldMap->chunkCountX = chunkCountX;
    newWorldMap->chunkCountY = chunkCountY;
    newWorldMap->mapWidth = chunkSize * chunkCountX;
    newWorldMap->mapHeight = chunkSize * chunkCountY;
    newWorldMap->cellsPerChunk = chunkSize * chunkSize;
    newWorldMap->cellDataSize = cellDataSize;
    newWorldMap->chunks = calloc(chunkCountX * chunkCountY, sizeof(htw_Chunk));

    for (int i = 0; i < chunkCountX * chunkCountY; i++) {
        newWorldMap->chunks[i].cellData = calloc(chunkSize * chunkSize, cellDataSize);
    }

    return newWorldMap;
}

void *htw_geo_getCell(htw_ChunkMap *chunkMap, htw_geo_GridCoord cellCoord) {
    u32 chunkIndex, cellIndex;
    htw_geo_gridCoordinateToChunkAndCellIndex(chunkMap, cellCoord, &chunkIndex, &cellIndex);
    return chunkMap->chunks[chunkIndex].cellData + (cellIndex * chunkMap->cellDataSize);
}

u32 htw_geo_getChunkIndexByChunkCoordinates(htw_ChunkMap *chunkMap, htw_geo_GridCoord chunkCoord) {
    // FIXME: this will not work if chunkCoord < -chunkCount, result will be % of a negative number (UB)
    // horizontal wrap
    chunkCoord.x += chunkMap->chunkCountX; // to account for negative chunkX
    chunkCoord.x %= chunkMap->chunkCountX;
    // vertical wrap
    chunkCoord.y += chunkMap->chunkCountY;
    chunkCoord.y %= chunkMap->chunkCountY;
    u32 chunkIndex = (chunkCoord.y * chunkMap->chunkCountX) + chunkCoord.x;
    return chunkIndex;
}

u32 htw_geo_getChunkIndexByGridCoordinates(htw_ChunkMap *chunkMap, htw_geo_GridCoord gridCoord) {
    htw_geo_GridCoord chunkCoord = {
        .x = gridCoord.x / chunkMap->chunkSize,
        .y = gridCoord.y / chunkMap->chunkSize
    };
    return htw_geo_getChunkIndexByChunkCoordinates(chunkMap, chunkCoord);
}

u32 htw_geo_getChunkIndexAtOffset(htw_ChunkMap *chunkMap, u32 startingChunk, htw_geo_GridCoord chunkOffset) {
    htw_geo_GridCoord chunkCoord = htw_geo_chunkIndexToChunkCoordinates(chunkMap, startingChunk);
    chunkCoord = htw_geo_addGridCoords(chunkCoord, chunkOffset);
    return htw_geo_getChunkIndexByChunkCoordinates(chunkMap, chunkCoord);
}

htw_geo_GridCoord htw_geo_chunkIndexToChunkCoordinates(htw_ChunkMap *chunkMap, u32 chunkIndex) {
    return (htw_geo_GridCoord){
        .x = chunkIndex % chunkMap->chunkCountX,
        .y = chunkIndex / chunkMap->chunkCountX
    };
}

void htw_geo_gridCoordinateToChunkAndCellIndex(htw_ChunkMap *chunkMap, htw_geo_GridCoord gridCoord, u32 *chunkIndex, u32 *cellIndex) {
    gridCoord = htw_geo_wrapGridCoordOnChunkMap(chunkMap, gridCoord);

    *chunkIndex = htw_geo_getChunkIndexByGridCoordinates(chunkMap, gridCoord);
    htw_geo_GridCoord chunkCoord = htw_geo_chunkIndexToChunkCoordinates(chunkMap, *chunkIndex);
    htw_geo_GridCoord cellCoord = {
        .x = gridCoord.x - (chunkCoord.x * chunkMap->chunkSize),
        .y = gridCoord.y - (chunkCoord.y * chunkMap->chunkSize)
    };
    *cellIndex = cellCoord.x + (cellCoord.y * chunkMap->chunkSize);
}

htw_geo_GridCoord htw_geo_chunkAndCellToGridCoordinates(htw_ChunkMap *chunkMap, u32 chunkIndex, u32 cellIndex) {
    u32 chunkX = chunkIndex % chunkMap->chunkCountX;
    u32 chunkY = chunkIndex / chunkMap->chunkCountX;
    u32 cellX = cellIndex % chunkMap->chunkSize;
    u32 cellY = cellIndex / chunkMap->chunkSize;
    htw_geo_GridCoord worldCoord = {
        .x = (chunkX * chunkMap->chunkSize) + cellX,
        .y = (chunkY * chunkMap->chunkSize) + cellY
    };
    return worldCoord;
}

void htw_geo_getChunkRootPosition(htw_ChunkMap *chunkMap, u32 chunkIndex, float *worldX, float *worldY) {
    u32 chunkX = chunkIndex % chunkMap->chunkCountX;
    u32 chunkY = chunkIndex / chunkMap->chunkCountX;
    s32 gridX = chunkX * chunkMap->chunkSize;
    s32 gridY = chunkY * chunkMap->chunkSize;
    htw_geo_getHexCellPositionSkewed((htw_geo_GridCoord){gridX, gridY}, worldX, worldY);
}

htw_geo_GridCoord htw_geo_wrapGridCoordOnChunkMap(htw_ChunkMap *chunkMap, htw_geo_GridCoord coord) {
    // wrap coordinates by chunkMap dimensions; add width first to handle negatives
    // FIXME: this will not work if coord < -chunkMap size, result will be % of a negative number (UB)
    coord.x += chunkMap->mapWidth;
    coord.x %= chunkMap->mapWidth;
    coord.y += chunkMap->mapHeight;
    coord.y %= chunkMap->mapHeight;
    return coord;
}

htw_geo_GridCoord htw_geo_addGridCoordsWrapped(htw_ChunkMap *chunkMap, htw_geo_GridCoord a, htw_geo_GridCoord b) {
    htw_geo_GridCoord result = htw_geo_addGridCoords(a, b);
    return htw_geo_wrapGridCoordOnChunkMap(chunkMap, result);
}

u32 htw_geo_getChunkMapHexDistance(htw_ChunkMap *chunkMap, htw_geo_GridCoord a, htw_geo_GridCoord b) {
    // To resolve sDist properly, need to determine which of the 9 possible 'regions' to shift b into
    // First, determine for each axis which shift (+, -, or 0) is closer
    const s32 shifts[] = {-1, 0, 1};
    s32 qShift = 0;
    s32 rShift = 0;
    s32 d1 = abs(a.x - b.x - chunkMap->mapWidth);
    s32 d2 = abs(a.x - b.x);
    s32 d3 = abs(a.x - b.x + chunkMap->mapWidth);
    s32 qDist = abs(a.x - b.x);
    if (qDist > chunkMap->mapWidth / 2) {
        qDist = chunkMap->mapWidth - qDist;
        qShift = chunkMap->mapWidth;
    }
    s32 rDist = abs(a.y - b.y);
    if (rDist > chunkMap->mapHeight / 2) {
        rDist = chunkMap->mapHeight - rDist;
        rShift = chunkMap->mapHeight;
    }
    s32 sDist = abs(a.x + a.y - (b.x + qShift) - (b.y + rShift));
    return (qDist + rDist + sDist) / 2;
}

// More complicated than a typical wrapped space distance check because of the effect y position has on x offset
// First determine which offset directions to use: only one (+ or -) in each axis can potentially put point b closer to a than it is already
// Next find the simple distance, and the distance after moving point b into the nearby 'parallel dimensions' indicated by the offest directions
// Return the lowest distance found
// NOTE: maximum possible distance between 2 points in a wrapping hexmap is (mapWidth / 2.0) / cos(PI / 6.0), or the distance from corner to center of triangular area
float htw_geo_hexCartesianDistance(htw_ChunkMap *chunkMap, htw_geo_GridCoord a, htw_geo_GridCoord b) {
    float extentX = htw_geo_getHexPositionX(chunkMap->mapWidth, 0);
    float extentY = htw_geo_getHexPositionY(chunkMap->mapHeight);
    // How far to offset x when wrapping around vertically
    float skewX = htw_geo_getHexPositionX(0, chunkMap->mapHeight);
    float xOffsetDir = a.x > b.x ? 1.0 : -1.0;
    float yOffsetDir = a.y > b.y ? 1.0 : -1.0;

    float x1, y1, x2, y2;
    htw_geo_getHexCellPositionSkewed(a, &x1, &y1);
    htw_geo_getHexCellPositionSkewed(b, &x2, &y2);

    float yDist0 = fabsf(y1 - y2);
    float yDist1 = fabsf(y1 - (y2 + (yOffsetDir * extentY)));

    float xDist00 = fabsf(x1 - x2);
    float xDist10 = fabsf(x1 - (x2 + (xOffsetDir * extentX)));
    float xDist01 = fabsf(x1 - (x2 + (yOffsetDir * skewX)));
    float xDist11 = fabsf(x1 - (x2 + (xOffsetDir * extentX) + (yOffsetDir * skewX)));

    float dist00 = (xDist00 * xDist00) + (yDist0 * yDist0);
    float dist10 = (xDist10 * xDist10) + (yDist0 * yDist0);
    float dist01 = (xDist01 * xDist01) + (yDist1 * yDist1);
    float dist11 = (xDist11 * xDist11) + (yDist1 * yDist1);

    float dist = fminf(fminf(dist00, dist10), fminf(dist01, dist11));
    return sqrtf(dist);
}
