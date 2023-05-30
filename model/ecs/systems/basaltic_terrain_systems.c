#include "basaltic_terrain_systems.h"
#include "basaltic_phases.h"
#include "components/basaltic_components_planes.h"
#include "htw_core.h"
#include "basaltic_worldState.h"
#include "basaltic_worldGen.h"
#include "khash.h"


void TerrainHourlyStep(ecs_iter_t *it);
void TerrainSeasonalStep(ecs_iter_t *it);
void CleanEmptyRoots(ecs_iter_t *it);

void chunkUpdate(htw_Chunk *chunk, size_t cellCount);

void TerrainHourlyStep(ecs_iter_t *it) {
    // TODO: are there any terrain updates that need to be make hourly (per-step)? If not, just make day/week/month/year step systems
}

void TerrainSeasonalStep(ecs_iter_t *it) {
    Plane *planes = ecs_field(it, Plane, 1);

    for (int i = 0; i < it->count; i++) {
        htw_ChunkMap *cm = planes[i].chunkMap;
        for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
            for (int x = 0; x < cm->chunkCountX; x++, c++) {
                chunkUpdate(&cm->chunks[c], cm->cellsPerChunk);
            }
        }
    }
}

void chunkUpdate(htw_Chunk *chunk, size_t cellCount) {
    CellData *base = chunk->cellData;
    for (int c = 0; c < cellCount; c++) {
        CellData *cell = &base[c];

        // Calc rain probabality and volume TODO: should be agent-based instead of part of the terrain update, but good enough for testing


        // TODO: need better defined growth behavior
        cell->groundwater += htw_randRange(256) < cell->surfacewater ? 1 : 0;
        if (cell->groundwater > 0) {
            cell->understory += 1;
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
