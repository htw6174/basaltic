#include "basaltic_worldGen.h"
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "basaltic_worldState.h"
#include "components/basaltic_components_planes.h"

void bc_elevationBrush(htw_ChunkMap *chunkMap, htw_geo_GridCoord pos, s32 value, u32 radius) {
    u32 area = htw_geo_getHexArea(radius);
    htw_geo_CubeCoord start = htw_geo_gridToCubeCoord(pos);
    htw_geo_CubeCoord relative = {0, 0, 0};
    for (int i = 0; i < area; i++) {
        //u32 dist = htw_geo_hexMagnitude(relative);
        float dist = htw_geo_hexCartesianDistance(chunkMap, (htw_geo_GridCoord){0, 0}, htw_geo_cubeToGridCoord(relative));
        float curve = 1.0 - (1.0 * (dist / radius));
        curve *= curve;
        s32 valueHere = curve * value + htw_randInt(2);
        htw_geo_CubeCoord worldCubeCoord = htw_geo_addCubeCoords(start, relative);
        htw_geo_GridCoord worldCoord = htw_geo_cubeToGridCoord(worldCubeCoord);
        CellData *cellData = htw_geo_getCell(chunkMap, worldCoord);
        cellData->height = MAX(cellData->height, valueHere);
        htw_geo_getNextHexSpiralCoord(&relative);
    }
}

void bc_wobbleLine(htw_ChunkMap *chunkMap, htw_geo_GridCoord startPos, u32 lineLength, u32 maxValue) {
    htw_geo_GridCoord cellPos = startPos;
    s32 nextDirIndex = htw_randIndex(HEX_DIRECTION_COUNT);
    for (int i = 0; i < lineLength; i++) {
        //bc_CellData *cellData = htw_geo_getCell(chunkMap, cellPos);
        // 0 at the start and end, maxValue in the middle
        s32 valueHere = maxValue - abs((s32)(maxValue - (2 * (maxValue * ((float)i / lineLength)))));
        //cellData->height = valueHere;
        bc_elevationBrush(chunkMap, cellPos, valueHere, valueHere / 3);
        htw_geo_GridCoord nextDir = htw_geo_hexGridDirections[nextDirIndex];
        cellPos = htw_geo_addGridCoords(cellPos, nextDir);// htw_geo_mulGridCoords(nextDir, 2));
        if (htw_randInt(2)){
            // add hex dir count to avoid taking mod of negative number
            nextDirIndex = (nextDirIndex + HEX_DIRECTION_COUNT + htw_randInt(2) - 1) % HEX_DIRECTION_COUNT; // only pick from adjacent directions (-1, 0, or 1 relative to last direction)
        }
    }
}

void bc_seedMountains(htw_ChunkMap *chunkMap, u32 mountainRangeCount, u32 rangeSize, u32 maxElevation) {
    for (int i = 0; i < mountainRangeCount; i++) {
        htw_geo_GridCoord startPos = {
            .x = htw_randIndex(chunkMap->mapWidth),
            .y = htw_randIndex(chunkMap->mapHeight)
        };
        u32 elevation = maxElevation - htw_randInt(maxElevation / 2);
        bc_wobbleLine(chunkMap, startPos, rangeSize, elevation);
    }
}

void bc_growMountains(htw_ChunkMap *chunkMap, float slope) {
    u32 cellsPerChunk = chunkMap->chunkSize * chunkMap->chunkSize;
    for (int c = 0, y = 0; y < chunkMap->chunkCountY; y++) {
        for (int x = 0; x < chunkMap->chunkCountX; x++, c++) {
            CellData *cellData = chunkMap->chunks[c].cellData;
            for (int i = 0; i < cellsPerChunk; i++) {
                CellData *cell = &cellData[i];
                htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(chunkMap, c, i);
            }
        }
    }
}


htw_ChunkMap *bc_createTerrain(u32 chunkCountX, u32 chunkCountY) {
    htw_ChunkMap *cm = htw_geo_createChunkMap(bc_chunkSize, chunkCountX, chunkCountY, sizeof(CellData));
    return cm;
}

void bc_generateTerrain(htw_ChunkMap *cm, u32 seed) {
    u32 width = bc_chunkSize;
    u32 height = bc_chunkSize;
    u32 cellsPerChunk = width * height;

    for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
        for (int x = 0; x < cm->chunkCountX; x++, c++) {
            CellData *cellData = cm->chunks[c].cellData;

            for (int i = 0; i < cellsPerChunk; i++) {
                CellData *cell = &cellData[i];
                htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, c, i);
                // noise values in 0..1
                float baseNoise = htw_geo_simplex(cm, cellCoord, seed, 8, 8);
                float nutrientNoise = htw_geo_simplex(cm, cellCoord, seed + 1, 4, 16);
                float rainNoise = htw_geo_simplex(cm, cellCoord, seed + 2, 4, 4);
                cell->height = (baseNoise - 0.5) * 64;
                cell->visibility = 0;
                cell->geology = (CellGeology){0};
                cell->tracks = 0;
                cell->groundwater = rainNoise * INT16_MAX / 8;
                cell->surfacewater = rainNoise * UINT16_MAX / 16;
                cell->humidityPreference = rainNoise * UINT16_MAX;
                cell->understory = nutrientNoise * (float)UINT32_MAX / 16;
                cell->canopy = nutrientNoise * (float)UINT32_MAX / 128;
            }
        }
    }
}
