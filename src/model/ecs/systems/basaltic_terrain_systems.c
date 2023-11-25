#include "basaltic_terrain_systems.h"
#include "basaltic_phases.h"
#include "basaltic_components.h"
#include "components/basaltic_components_planes.h"
#include "htw_core.h"
#include "basaltic_worldState.h"
#include "basaltic_worldGen.h"
#include "khash.h"

#define MAX_DROUGHT_TOLERANCE (1<<13)

void chunkDailyUpdate(Plane *plane, const Climate *climate, size_t chunkIndex);

void TickSeasons(ecs_iter_t *it) {
    Climate *climates = ecs_field(it, Climate, 1);

    for (int i = 0; i < it->count; i++) {
        Season *season = &climates[i].season;
        season->cycleProgress = (season->cycleProgress + 1) % season->cycleLength;
        season->temperatureModifier =
            sinf( ((float)season->cycleProgress * PI) / season->cycleLength ) * season->temperatureRange;
    }
}

void TerrainDailyStep(ecs_iter_t *it) {
    Plane *planes = ecs_field(it, Plane, 1);
    Climate *climates = ecs_field(it, Climate, 2);

    for (int i = 0; i < it->count; i++) {
        htw_ChunkMap *cm = planes[i].chunkMap;
        for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
            for (int x = 0; x < cm->chunkCountX; x++, c++) {
                chunkDailyUpdate(&planes[i], &climates[i], c);
            }
        }
    }
}

void chunkDailyUpdate(Plane *plane, const Climate *climate, size_t chunkIndex) {
    htw_ChunkMap *cm = plane->chunkMap;
    CellData *base = cm->chunks[chunkIndex].cellData;

    for (int c = 0; c < cm->cellsPerChunk; c++) {
        CellData *cell = &base[c];
        htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, chunkIndex, c);

        // don't need to do anything if cell is below sea level
        if (cell->height < 0) {
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

        // Calc rain probabality and volume TODO: should be agent-based instead of part of the terrain update, but good enough for testing
        // if (htw_randRange(256 * 128) < cell->surfacewater) {
        //     // "rainstorm"; if less than 0 (tracking number of dry hours), then start from 0
        //     cell->groundwater = min_int(max_int(cell->surfacewater, cell->groundwater + cell->surfacewater), 512);
        // }

        // erase tracks
        tracks = MIN(tracks * 0.95, tracks - 1);

        // surface water evaporates and filters into ground water TODO: variable rates depending on vegetation
        if (surfacewater > 0) {
            s64 evaporate = 24;
            s64 infiltrate = MIN(24, surfacewater); // limit to available surface water
            surfacewater -= evaporate + infiltrate;
            groundwater += infiltrate;
        }

        if (groundwater <= 0) {
            // Track hours since water was available
            groundwater -= 24;
            // kill vegetation if dry longer than drought resistance threshold
            if (-cell->groundwater > (MAX_DROUGHT_TOLERANCE - cell->humidityPreference)) {
                // lower humidity preference
                humidityPreference -= 1;
                understory *= 0.98;
                canopy *= 0.98;
            }
        } else if (isGrowingSeason) {
            // raise humidity preference
            humidityPreference += 1;
            // Remove groundwater according to evapotranspiration TODO
            groundwater -= 24;
            // grow new vegetation
            // 100 days to reach full understory
            understory += (0.01 * UINT32_MAX);

            // should take ~100 years to reach full canopy, but numbers are less clear because of variable growth rate
            float canopyGrowth = plane_CanopyGrowthRate(plane, cellCoord);
            canopy += (canopyGrowth * 0.0005 * UINT32_MAX);
        } else {
            // dry weather outside of growing season doesn't effect humidity preference
            // Remove groundwater according to evaporation TODO
            groundwater -= 24;
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

    ECS_SYSTEM(world, TickSeasons, AdvanceStep, [inout] Climate);

    ECS_SYSTEM(world, TerrainDailyStep, AdvanceStep,
        [inout] Plane,
        [in] Climate
    );
    ecs_set_tick_source(world, TerrainDailyStep, TickDay);

    ECS_SYSTEM(world, CleanEmptyRoots, Cleanup, bc.planes.CellRoot);
}
