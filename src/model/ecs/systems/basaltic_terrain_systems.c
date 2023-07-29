#include "basaltic_terrain_systems.h"
#include "basaltic_phases.h"
#include "basaltic_components.h"
#include "components/basaltic_components_planes.h"
#include "htw_core.h"
#include "basaltic_worldState.h"
#include "basaltic_worldGen.h"
#include "khash.h"


void TerrainHourlyStep(ecs_iter_t *it);
void TerrainSeasonalStep(ecs_iter_t *it);
void CleanEmptyRoots(ecs_iter_t *it);

void chunkUpdate(Plane *plane, u64 step, size_t chunkIndex);

void TerrainHourlyStep(ecs_iter_t *it) {
    // TODO: are there any terrain updates that need to be make hourly (per-step)? If not, just make day/week/month/year step systems
}

void TerrainSeasonalStep(ecs_iter_t *it) {
    Plane *planes = ecs_field(it, Plane, 1);
    u64 step = ecs_singleton_get(it->world, Step)->step;

    for (int i = 0; i < it->count; i++) {
        htw_ChunkMap *cm = planes[i].chunkMap;
        for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
            for (int x = 0; x < cm->chunkCountX; x++, c++) {
                chunkUpdate(&planes[i], step, c);
            }
        }
    }
}

void chunkUpdate(Plane *plane, u64 step, size_t chunkIndex) {
    htw_ChunkMap *cm = plane->chunkMap;
    CellData *base = cm->chunks[chunkIndex].cellData;
    for (int c = 0; c < cm->cellsPerChunk; c++) {
        CellData *cell = &base[c];
        htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, chunkIndex, c);

        s32 biotemp = plane_GetCellBiotemperature(plane, cellCoord);
        float relativeTemp = sinf((float)step * 2.0 * 3.14169 / 240.0) * 1000.0; // in centacelsius, +/- 10 deg
        float realTemp = (float)biotemp + relativeTemp;
        bool isGrowingSeason = realTemp > 0.0 && realTemp < 3000.0;

        // Calc rain probabality and volume TODO: should be agent-based instead of part of the terrain update, but good enough for testing
        if (htw_randRange(256 * 128) < cell->surfacewater) {
            // "rainstorm"; if less than 0 (tracking number of dry hours), then start from 0
            cell->groundwater = min_int(max_int(cell->surfacewater, cell->groundwater + cell->surfacewater), 512);
        }

        bool isWaterAvailable = cell->groundwater > 0;

        if (!isWaterAvailable) {
            // Track hours since water was available
            cell->groundwater--;
            // kill vegetation if dryer longer than drought resistance threshold
            if (abs(cell->groundwater) > (256 - cell->humidityPreference)) {
                // Make sure decrease doesn't cause underflow
                cell->understory = cell->understory < cell->understory - 1 ? 0 : cell->understory - 1;
                cell->canopy = cell->canopy < cell->canopy - 1 ? 0 : cell->canopy - 1;
            }
        }

        if (isWaterAvailable && isGrowingSeason && cell->height >= 0) {
            // Remove groundwater according to evapotranspiration
            cell->groundwater--;
            // grow new vegetation
            cell->understory += 1;
            float canopyGrowthProb = plane_CanopyGrowthRate(plane, cellCoord);
            cell->canopy += htw_randRange(256) < (canopyGrowthProb * 256.0) ? 1 : 0;
        } else {
            // Remove groundwater according to evaporation
            cell->groundwater--;
        }
    }
}

void CleanEmptyRoots(ecs_iter_t *it) {
    for (int i = 0; i < it->count; i++) {
        // Remove root if no more children
        u32 childCount = 0;
        ecs_iter_t children = ecs_children(it->world, it->entities[i]);
        while (ecs_iter_next(&children)) {
            for (int i = 0; i < children.count; i++) {
                childCount++;
            }
        }
        if (childCount == 0) {
            ecs_delete(it->world, it->entities[i]);
        }
    }
}

void BcSystemsTerrainImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystemsTerrain);

    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);

    ECS_SYSTEM(world, TerrainSeasonalStep, AdvanceHour, Plane);
    ECS_SYSTEM(world, CleanEmptyRoots, Cleanup, bc.planes.CellRoot);
}
