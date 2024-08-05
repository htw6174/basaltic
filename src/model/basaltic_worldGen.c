#include "basaltic_worldGen.h"
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "ecs/components/basaltic_components_planes.h"

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


htw_ChunkMap *bc_createTerrain(u32 chunkSize, u32 chunkCountX, u32 chunkCountY) {
    htw_ChunkMap *cm = htw_geo_createChunkMap(chunkSize, chunkCountX, chunkCountY, sizeof(CellData));
    return cm;
}

void bc_generateTerrain(htw_ChunkMap *cm, u32 seed) {
    for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
        for (int x = 0; x < cm->chunkCountX; x++, c++) {
            CellData *cellData = cm->chunks[c].cellData;

            for (int i = 0; i < cm->cellsPerChunk; i++) {
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
                cell->groundwater = rainNoise * INT16_MAX / 128;
                cell->surfacewater = 0; //rainNoise * UINT16_MAX / 256;
                cell->humidityPreference = rainNoise * 8192;
                cell->understory = nutrientNoise * (float)UINT32_MAX / 16;
                cell->canopy = nutrientNoise * (float)UINT32_MAX / 128;
            }
        }
    }
}

/* Rivers */

/// Stored in shortestLeft and shorestRight: number of sides between reference direction corner on that side and the closest connection on that side. -1 if no connection. 0 if no segments needed to connect.
/// return: true = there is a preferred connection side; false = no preferred side
bool shortestDistanceToConnections(const CellWaterConnections *connections, s32 *shortestLeft, s32 *shortestRight) {
    // Smallest number of segments between the new connection's lefthand (out) corner and the rightmost connection of any other edge
    s32 minDistLeft = 3;
    // Smallest number of segments between the new connection's righthand (in) corner and the leftmost connection of any other edge
    s32 minDistRight = 3;
    for (int i = 1; i < HEX_DIRECTION_COUNT; i++) {
        bool conLeft = connections->connectionsOut[i] > 0;
        bool conRight = connections->connectionsIn[i] > 0;
        s32 distRight = minDistRight;
        if (conLeft) {
            distRight = i - 1;
        } else if (conRight) {
            distRight = i;
        }
        minDistRight = MIN(minDistRight, distRight);

        s32 distLeft = minDistLeft;
        if (conRight) {
            distLeft = HEX_DIRECTION_COUNT - (i + 1);
        } else if (conLeft) {
            distLeft = HEX_DIRECTION_COUNT - i;
        }
        minDistLeft = MIN(minDistLeft, distLeft);
    }
    // -1 if no connection on that side
    *shortestLeft = minDistLeft < 3 ? minDistLeft : -6;
    *shortestRight = minDistRight < 3 ? minDistRight : -6;
    return minDistLeft != minDistRight;
}

void createSegmentPath(CellWaterConnections *connections, s32 segmentsLeft, s32 segmentsRight) {
    // TODO: should have a tool setting that controls how segments are created between connections, allow bias toward straight or curvy rivers by increasing minimum distance threshold
    for (int i = 0; i < segmentsRight; i++) {
        connections->segments[i + 1] = true;
    }
    for (int i = 0; i < segmentsLeft; i++) {
        connections->segments[HEX_DIRECTION_COUNT - (i + 1)] = true;
    }
}

void cleanSegmentPath(CellWaterConnections *connections) {
    // Clean only outward from edge 0 of connections
    // to the left, all the way around:
    for (int i = 0; i < HEX_DIRECTION_COUNT; i++) {
        // If left end of segment has no connections and no adjoining segment, remove the segment
        s32 iLeft = htw_geo_hexDirectionLeft(i);
        bool hasSegmentToLeft = connections->segments[iLeft];
        bool hasInConnection = connections->connectionsIn[iLeft];
        bool hasOutConnection = connections->connectionsOut[i];
        connections->segments[i] &= hasSegmentToLeft || hasInConnection || hasOutConnection;
    }
    // to the right, all the way around
    for (int i = HEX_DIRECTION_COUNT - 1; i >= 0; i--) {
        // If left end of segment has no connections and no adjoining segment, remove the segment
        s32 iRight = htw_geo_hexDirectionRight(i);
        bool hasSegmentToLeft = connections->segments[iRight];
        bool hasInConnection = connections->connectionsIn[i];
        bool hasOutConnection = connections->connectionsOut[iRight];
        connections->segments[i] &= hasSegmentToLeft || hasInConnection || hasOutConnection;
    }
}

void extractCellWaterway(const htw_ChunkMap *cm, htw_geo_GridCoord position, HexDirection referenceDirection, const CellData *cell, CellWaterConnections *connections) {
    u32 ww = *(u32*)&(cell->waterways);
    for (int i = 0; i < HEX_DIRECTION_COUNT; i++) {
        HexDirection dir = (referenceDirection + i) % HEX_DIRECTION_COUNT;
        u32 shift = dir * 4;
        u32 edge = ww >> shift;
        u32 connectionMask = (1 << 2) - 1; // lo 3 bits
        u32 segmentMask = 1 << 3; // 4th bit
        u32 connection = edge & connectionMask;
        u32 segment = edge & segmentMask;

        connections->connectionsOut[i] = connection;
        connections->segments[i] = segment != 0;

        // Get connection data from neighbor cell in direction, in side opposite direction
        connections->neighbors[i] = htw_geo_getCell(cm, POSITION_IN_DIRECTION(position, dir));
        u32 wn = *(u32*)&(connections->neighbors[i]->waterways);
        dir = htw_geo_hexDirectionOpposite(dir);
        shift = dir * 4;
        edge = wn >> shift;
        connection = edge & connectionMask;
        connections->connectionsIn[i] = connection;
    }
}

void storeCellWaterway(htw_ChunkMap *cm, HexDirection referenceDirection, CellData *cell, const CellWaterConnections *connections) {
    u32 ww = 0; // new blank CellWaterways
    for (int i = 0; i < HEX_DIRECTION_COUNT; i++) {
        HexDirection dir = (referenceDirection + i) % HEX_DIRECTION_COUNT;
        u32 shift = dir * 4;
        u32 connection = MIN(connections->connectionsOut[i], 7u);
        u32 segment = connections->segments[i] << 3;
        u32 edge = connection | segment;
        // merge in edge
        ww |= edge << shift;
    }
    cell->waterways = *(CellWaterways*)&ww;
}

bool bc_hasAnyWaterways(CellWaterways waterways) {
    static const CellWaterways emptyWW = {0};
    return memcmp(&waterways, &emptyWW, sizeof(CellWaterways)) != 0;
}

CellWaterConnections bc_extractCellWaterways(const htw_ChunkMap *cm, htw_geo_GridCoord position) {
    CellWaterConnections cw;
    CellData *cell = htw_geo_getCell(cm, position);
    extractCellWaterway(cm, position, 0, cell, &cw);
    return cw;
}

RiverConnection bc_riverConnectionFromCells(const htw_ChunkMap *cm, htw_geo_GridCoord a, htw_geo_GridCoord b) {
    RiverConnection rc;
    CellData *c1 = htw_geo_getCell(cm, a);
    CellData *c2 = htw_geo_getCell(cm, b);

    // for convience, ensure a is coord of uphill cell
    if (c2->height > c1->height) {
        rc.uphillCell = c2;
        rc.downhillCell = c1;
        htw_geo_GridCoord temp = a;
        a = b;
        b = temp;
    } else {
        rc.uphillCell = c1;
        rc.downhillCell = c2;
    }
    // wrap difference to ensure relative direction is correct
    htw_geo_GridCoord wrappedVec = htw_geo_wrapVectorOnChunkMap(cm, htw_geo_subGridCoords(b, a));
    rc.upToDownDirection = htw_geo_vectorHexDirection(wrappedVec);

    // extract info for uphill cell
    extractCellWaterway(cm, a, rc.upToDownDirection, rc.uphillCell, &rc.uphill);
    // for downhill
    extractCellWaterway(cm, b, htw_geo_hexDirectionOpposite(rc.upToDownDirection), rc.downhillCell, &rc.downhill);

    return rc;
}

void bc_applyRiverConnection(htw_ChunkMap *cm, const RiverConnection *rc) {
    storeCellWaterway(cm, rc->upToDownDirection, rc->uphillCell, &rc->uphill);
    storeCellWaterway(cm, htw_geo_hexDirectionOpposite(rc->upToDownDirection), rc->downhillCell, &rc->downhill);
}

void bc_makeRiverConnection(htw_ChunkMap *cm, htw_geo_GridCoord a, htw_geo_GridCoord b, u8 size) {
    RiverConnection rc = bc_riverConnectionFromCells(cm, a, b);
    size = MIN(size, 7);

    // TODO: arg to control how many connections are made
    s32 uphillDistLeft, uphillDistRight, downhillDistLeft, downhillDistRight;
    bool uphillPreferred = shortestDistanceToConnections(&rc.uphill, &uphillDistLeft, &uphillDistRight);
    bool downhillPreferred = shortestDistanceToConnections(&rc.downhill, &downhillDistLeft, &downhillDistRight);
    if (uphillPreferred) {
        if (abs(uphillDistLeft) < abs(uphillDistRight)) {
            rc.uphill.connectionsOut[0] = size;
            uphillDistRight = 0;
        } else {
            rc.downhill.connectionsOut[0] = size;
            uphillDistLeft = 0;
        }
    } else if (downhillPreferred) {
        // when connection side is determined by downhill, still need to set one or the other uphill distance to 0
        if (abs(downhillDistLeft) < abs(downhillDistRight)) {
            rc.downhill.connectionsOut[0] = size;
            uphillDistLeft = 0;
        } else {
            rc.uphill.connectionsOut[0] = size;
            uphillDistRight = 0;
        }
    } else {
        // connect on random or constant side
        rc.uphill.connectionsOut[0] = size;
        uphillDistRight = 0;
    }
    createSegmentPath(&rc.uphill, uphillDistLeft, uphillDistRight);
    // determine how to connect downhill
    // distance to existing connection is increased if new connection is on opposite side
    s32 extraLeft = 0, extraRight = 0;
    if (rc.uphill.connectionsOut[0]) {
        extraLeft += 1;
    }
    if (rc.downhill.connectionsOut[0]) {
        extraRight += 1;
    }
    if (abs(downhillDistLeft) + extraLeft < abs(downhillDistRight) + extraRight) {
        downhillDistRight = 0;
        if (extraLeft > 0) {
            rc.downhill.segments[0] = true;
        }
    } else {
        downhillDistLeft = 0;
        if (extraRight > 0) {
            rc.downhill.segments[0] = true;
        }
    }
    createSegmentPath(&rc.downhill, downhillDistLeft, downhillDistRight);
    // TODO: should have arg that controls if segment is created on the edge(s) being connected

    bc_applyRiverConnection(cm, &rc);
}

void bc_removeRiverConnection(htw_ChunkMap *cm, htw_geo_GridCoord a, htw_geo_GridCoord b) {
    RiverConnection rc = bc_riverConnectionFromCells(cm, a, b);

    // remove edge connections between cells
    // NOTE: in connections aren't applied back to the cell, but still need to set them here so that segment cleanup can see that they are removed
    rc.uphill.connectionsOut[0] = 0;
    rc.uphill.connectionsIn[0] = 0;
    rc.downhill.connectionsOut[0] = 0;
    rc.downhill.connectionsIn[0] = 0;
    // Remove river segments that are no longer linking edge connections
    cleanSegmentPath(&rc.uphill);
    cleanSegmentPath(&rc.downhill);

    bc_applyRiverConnection(cm, &rc);
}


void bc_smoothTerrain(htw_ChunkMap *cm, s32 minProminance) {
    for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
        for (int x = 0; x < cm->chunkCountX; x++, c++) {
            CellData *cellData = cm->chunks[c].cellData;

            for (int i = 0; i < cm->cellsPerChunk; i++) {
                CellData *cell = &cellData[i];
                htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, c, i);

                CellData *lowestNeighbor;
                s32 lowestElevation = cell->height;
                s32 prominance = 0;
                for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
                    htw_geo_GridCoord neighborCoord = POSITION_IN_DIRECTION(cellCoord, d);
                    CellData *neighbor = htw_geo_getCell(cm, neighborCoord);
                    prominance += cell->height - neighbor->height;
                    if (neighbor->height < lowestElevation) {
                        lowestElevation = neighbor->height;
                        lowestNeighbor = neighbor;
                    }
                }

                if (prominance >= minProminance) {
                    cell->height--;
                    lowestNeighbor->height++;
                }
            }
        }
    }
}
