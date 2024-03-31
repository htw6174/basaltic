#include "basaltic_terrain_systems.h"
#include "basaltic_phases.h"
#include "basaltic_components.h"
#include "components/basaltic_components_planes.h"
#include "htw_core.h"
#include "basaltic_worldGen.h"
#include "khash.h"
#include <math.h>

#define MAX_DROUGHT_TOLERANCE (1<<13)

// water units in ML/km^2 aka 0.1 mL/cm^2; 1 ML == 1 mm of water over 1 km^2 (one tile area)
float infiltrationPerHour(s32 temp, s64 groundwater, s64 vegetation);
float surfaceEvaporationPerHour(s32 temp, s64 vegetation);
float groundEvaporationPerHour(s32 temp, s64 vegetation);
float evaporationPerHour(s32 temp, s64 vegetation);
float transpirationPerHour(s32 temp, s64 vegetation);
float potentialEvapotranspirationPerHour(s32 temp, s64 vegetation);
/// dT: number of hours to simulate for each cell
void chunkUpdate(Plane *plane, const Climate *climate, size_t chunkIndex, s64 dT);
void riverUpdate(Plane *plane, size_t chunkIndex, s64 dT);

// TODO: move these simulation rate functions into a public header and add a way to visualize any of them on terrain with a gradient

float infiltrationPerHour(s32 temp, s64 groundwater, s64 vegetation) {
    // No infiltration in freezing weather
    if (temp < 0) {
        return 0.0;
    }
    // range between 10 and 300 mm per hour
    // TODO: low when no or high groundwater, high somewhere in the middle
    // low when no vegetation, high when lots of vegetation
    float vegFactor = ((double)vegetation / UINT32_MAX);
    // TODO: factor in soil types?
    // baseline of 10 and no negative factors so there is always some
    return 10 + (vegFactor * 300);
}

float evaporationPerHour(s32 temp, s64 vegetation) {
    // based on global average between 0.03 and 0.3 mm/hour
    float tempFactor = ((float)temp / 4000.0);
    float vegFactor = ((double)vegetation / UINT32_MAX);
    float total = (tempFactor - (vegFactor / 4.0)) * 0.3;
    return MAX(0.0, total);
}

float transpirationPerHour(s32 temp, s64 vegetation) {
    float tempFactor = ((float)temp / 4000.0);
    float vegFactor = ((double)vegetation / UINT32_MAX);
    float total = (vegFactor + (tempFactor / 4.0)) * 0.3;
    return MAX(0.0, total);
}

void chunkUpdate(Plane *plane, const Climate *climate, const size_t chunkIndex, const s64 dT) {
    htw_ChunkMap *cm = plane->chunkMap;
    CellData *base = cm->chunks[chunkIndex].cellData;

    for (int c = 0; c < cm->cellsPerChunk; c++) {
        CellData *cell = &base[c];
        htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, chunkIndex, c);

        // don't need to do anything if cell is below sea level
        if (cell->height < 0) {
            cell->groundwater = 0;
            cell->surfacewater = 0;
            continue;
        }

        s32 temp = plane_GetCellTemperature(plane, climate, cellCoord);
        bool isGrowingSeason = temp > 0 && temp < 3000; // between 0 and 30 c; possibly too wide

        // Expand celldata fields to avoid overflows
        s64 tracks = cell->tracks;
        s64 groundwater = cell->groundwater;
        s64 surfacewater = cell->surfacewater;
        s64 humidityPreference = cell->humidityPreference;
        s64 understory = cell->understory;
        s64 canopy = cell->canopy;

        // erase tracks TODO: effect should scale with dT
        tracks = MIN(tracks * 0.98, tracks - 1);

        s64 vegetationCoverage = MAX(understory + canopy, UINT32_MAX);
        s64 evaporation = evaporationPerHour(temp, vegetationCoverage) * dT;

        // surface water enters the ground and evaporates
        if (surfacewater > 0) {
            s64 infiltration = infiltrationPerHour(temp, groundwater, vegetationCoverage) * dT;
            s64 groundAvailableCapacity = INT16_MAX - groundwater;
            s64 limit = MIN(surfacewater, groundAvailableCapacity); // can't transfer more than is available or more than destination can accept
            infiltration = MIN(infiltration, limit);
            // if groundwater in negative, start from 1 so frozen surface water doesn't 'dry out' the tile
            groundwater = MAX(1, groundwater) + infiltration;
            surfacewater -= evaporation + infiltration;
        }

        if (groundwater <= 0) {
            // Track hours since water was available
            groundwater -= dT;
            // kill vegetation if dry longer than drought resistance threshold
            if (-groundwater > (MAX_DROUGHT_TOLERANCE - humidityPreference)) {
                // lower humidity preference
                humidityPreference -= 1 * dT;
                // TODO: dieoff should scale with dT
                understory *= 0.98;
                canopy *= 0.98;
            }
        } else if (isGrowingSeason) {
            // raise humidity preference
            humidityPreference += 1 * dT;
            // Remove groundwater according to evapotranspiration
            s64 transpiration = transpirationPerHour(temp, vegetationCoverage);
            groundwater -= (evaporation + transpiration);
            // grow new vegetation
            // 100 days to reach full understory
            understory += (0.01 * UINT32_MAX);

            // should take ~100 years to reach full canopy, but numbers are less clear because of variable growth rate
            // TODO: high tracks % should slow early canopy growth, very high tracks % should slow understory growth
            float canopyGrowth = plane_CanopyGrowthRate(plane, cellCoord);
            canopy += (canopyGrowth * 0.0005 * UINT32_MAX);
        } else {
            // dry weather outside of growing season doesn't effect humidity preference
            // Remove groundwater according to evaporation TODO
            groundwater -= evaporation;
        }

        // Clamp values before assigning back to cell
        cell->tracks = CLAMP(tracks, 0, UINT16_MAX);
        cell->groundwater = CLAMP(groundwater, INT16_MIN, INT16_MAX);
        cell->surfacewater = CLAMP(surfacewater, 0, UINT16_MAX);
        cell->humidityPreference = CLAMP(humidityPreference, 0, UINT16_MAX);
        cell->understory = CLAMP(understory, 0, UINT32_MAX);
        cell->canopy = CLAMP(canopy, 0, UINT32_MAX);
    }
}

void riverConnectionsUpdate(Plane *plane, size_t chunkIndex) {
    htw_ChunkMap *cm = plane->chunkMap;
    CellData *base = cm->chunks[chunkIndex].cellData;

    for (int c = 0; c < cm->cellsPerChunk; c++) {
        CellData *cell = &base[c];
        htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, chunkIndex, c);

        // don't make rivers under sea level
        if (cell->height < 0) {
            continue;
        }

        // If more than 1/4 of max, form a river to lowest neighbor
        if (cell->groundwater > (INT16_MAX / 4)) {
            s32 lowest = cell->height;
            s32 lowestDir = -1;
            for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
                htw_geo_GridCoord neighborCoord = POSITION_IN_DIRECTION(cellCoord, d);
                CellData *neighborCell = htw_geo_getCell(cm, neighborCoord);
                if (neighborCell->height <= lowest) {
                    lowest = neighborCell->height;
                    lowestDir = d;
                }
            }
            if (lowestDir != -1) {
                // min at 8k, max above 65k
                // min river size at 8k (2^13), max above 8k*7 (2^16 - 2^13)
                s64 totalWater = (s64)cell->groundwater + (s64)cell->surfacewater;
                u8 size = totalWater / (1 << 13);
                // form river from cell to lowestDir
                htw_geo_GridCoord neighborCoord = POSITION_IN_DIRECTION(cellCoord, lowestDir);
                bc_makeRiverConnection(cm, cellCoord, neighborCoord, size);
            }

            /*
             *   // Mixed up pointers to different directions
             *   HexDirection candidates[HEX_DIRECTION_COUNT] = {1, 4, 5, 2, 3, 0};
             *   for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
             *       htw_geo_GridCoord neighborCoord = POSITION_IN_DIRECTION(cellCoord, d);
             *       CellData *neighborCell = htw_geo_getCell(cm, neighborCoord);
             *       if (neighborCell->height <= cell->height) {
             *           // make candidate self-reference to indicate that search can stop here
             *           candidates[d] = d;
        }
        }
        // hashed random starting point in candidates list
        u32 hash = xxh_hash2d(0, cellCoord.x, cellCoord.y) % HEX_DIRECTION_COUNT;
        HexDirection dir = candidates[hash];
        // try each candidate in mixed order until self reference found
        for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
            if (candidates[dir] == dir) {
                // form river from cell to lowestDir
                htw_geo_GridCoord neighborCoord = POSITION_IN_DIRECTION(cellCoord, dir);
                bc_makeRiverConnection(cm, cellCoord, neighborCoord);
                break;
        } else {
            dir = candidates[dir];
        }
        }
            */
        } else if (cell->groundwater < (-180 * 24) && bc_hasAnyWaterways(cell->waterways)) {
            // If 2 neighboring cells dry for more than 6 months, remove river connection
            for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
                htw_geo_GridCoord neighborCoord = POSITION_IN_DIRECTION(cellCoord, d);
                CellData *neighborCell = htw_geo_getCell(cm, neighborCoord);
                if (neighborCell->groundwater < (-180 * 24) && bc_hasAnyWaterways(neighborCell->waterways)) {
                    bc_removeRiverConnection(cm, cellCoord, neighborCoord);
                }
            }
        }
    }
}

void riverUpdate(Plane *plane, size_t chunkIndex, s64 dT) {
    htw_ChunkMap *cm = plane->chunkMap;
    CellData *base = cm->chunks[chunkIndex].cellData;

    for (int c = 0; c < cm->cellsPerChunk; c++) {
        CellData *cell = &base[c];
        htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, chunkIndex, c);

        // If any river connections, carry groundwater downhill
        // Only need to check 3 directions to operate on every pair of neighboring cells
        for (int d = 0; d < 3; d++) {
            htw_geo_GridCoord neighborCoord = POSITION_IN_DIRECTION(cellCoord, d);
            CellData *neighborCell = htw_geo_getCell(cm, neighborCoord);

            if (bc_hasAnyWaterways(cell->waterways) || bc_hasAnyWaterways(neighborCell->waterways)) {
                RiverConnection rc = bc_riverConnectionFromCells(cm, cellCoord, neighborCoord);

                s32 slope = rc.uphillCell->height - rc.downhillCell->height;
                // if no gradient, use side with higher total water as uphill
                if (slope == 0) {
                    s64 totalDown = rc.downhillCell->groundwater + rc.downhillCell->surfacewater;
                    s64 totalUp = rc.uphillCell->groundwater + rc.uphillCell->surfacewater;
                    if (totalDown > totalUp) {
                        CellData *temp = rc.uphillCell;
                        rc.uphillCell = rc.downhillCell;
                        rc.downhillCell = temp;
                    }
                }

                s64 source = rc.uphillCell->groundwater;
                s64 dest = rc.downhillCell->groundwater;

                // max volume determined by size of both connecting segments and velocity determined slope
                s32 inArea = rc.uphill.connectionsIn[0] * rc.uphill.connectionsIn[0];
                s32 outArea = rc.uphill.connectionsOut[0] * rc.uphill.connectionsOut[0];
                s64 volume = (float)(slope + 1) * (float)(inArea + outArea) * dT;
                // ignore negative groundwater on uphill side
                s64 available= MAX(0, source);
                // can't transport more than is available uphill
                volume = MIN(volume, available);

                // TODO: net volume as function of volume and total source water; should stop flowing before source completely dries up
                // TODO: should be able to flow from ground and surface water, flow from groundwater should be limited; overflow to dest should go to surface water

                // if some water going to dest, reset dry days counter
                dest = volume > 0 ? MAX(0, dest) : dest;

                source -= volume;
                dest += volume;

                rc.uphillCell->groundwater = CLAMP(source, INT16_MIN, INT16_MAX);
                rc.downhillCell->groundwater = CLAMP(dest, INT16_MIN, INT16_MAX);
            }
        }
    }
}

void TickSeasons(ecs_iter_t *it) {
    Climate *climates = ecs_field(it, Climate, 1);

    for (int i = 0; i < it->count; i++) {
        Season *season = &climates[i].season;
        season->cycleProgress = (season->cycleProgress + 1) % season->cycleLength;
        season->temperatureModifier =
        sinf( ((float)season->cycleProgress * TAU) / season->cycleLength ) * season->temperatureRange;
    }
}

void TerrainDailyStep(ecs_iter_t *it) {
    Plane *planes = ecs_field(it, Plane, 1);
    Climate *climates = ecs_field(it, Climate, 2);

    for (int i = 0; i < it->count; i++) {
        htw_ChunkMap *cm = planes[i].chunkMap;
        for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
            for (int x = 0; x < cm->chunkCountX; x++, c++) {
                chunkUpdate(&planes[i], &climates[i], c, 24);
            }
        }
    }
}

void FormRivers(ecs_iter_t *it) {
    Plane *planes = ecs_field(it, Plane, 1);

    for (int i = 0; i < it->count; i++) {
        htw_ChunkMap *cm = planes[i].chunkMap;
        for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
            for (int x = 0; x < cm->chunkCountX; x++, c++) {
                riverConnectionsUpdate(&planes[i], c);
            }
        }
    }
}

void FlowRivers(ecs_iter_t *it) {
    Plane *planes = ecs_field(it, Plane, 1);

    for (int i = 0; i < it->count; i++) {
        htw_ChunkMap *cm = planes[i].chunkMap;
        for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
            for (int x = 0; x < cm->chunkCountX; x++, c++) {
                riverUpdate(&planes[i], c, 24);
            }
        }
    }
}

void CleanEmptyRoots(ecs_iter_t *it) {
    for (int i = 0; i < it->count; i++) {
        // Remove root if no contianed entities
        ecs_iter_t ti = ecs_term_iter(it->world, &(ecs_term_t){
            .first = {.id = IsIn},
            .second = {.id = it->entities[i]}
        });
        if (ecs_iter_is_true(&ti)) {
            // at least one contained entity, don't delete
        } else {
            ecs_delete(it->world, it->entities[i]);
        }
    }
}

void BcSystemsTerrainImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystemsTerrain);

    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);

    ECS_SYSTEM(world, TickSeasons, AdvanceStep, [inout] Climate);

    ECS_SYSTEM(world, TerrainDailyStep, AdvanceStep,
        [inout] Plane,
        [in] Climate
    );
    ecs_set_tick_source(world, TerrainDailyStep, TickDay);

    ECS_SYSTEM(world, FormRivers, AdvanceStep,
        [inout] Plane,
    );
    ecs_set_tick_source(world, FormRivers, TickMonth);

    ECS_SYSTEM(world, FlowRivers, AdvanceStep,
        [inout] Plane,
    );
    ecs_set_tick_source(world, FlowRivers, TickDay);

    ECS_SYSTEM(world, CleanEmptyRoots, Cleanup, bc.planes.CellRoot);
}
