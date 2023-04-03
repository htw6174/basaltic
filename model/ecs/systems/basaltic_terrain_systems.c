#include "basaltic_terrain_systems.h"
#include "htw_core.h"
#include "basaltic_worldGen.h"
#include "khash.h"

void updateVegetation(bc_CellData *cellData);

ecs_entity_t bc_createTerrain(ecs_world_t *world, u32 chunkCountX, u32 chunkCountY, size_t maxEntities) {
    htw_ChunkMap *cm = htw_geo_createChunkMap(bc_chunkSize, chunkCountX, chunkCountY, sizeof(bc_CellData));
    khash_t(GridMap) *gm = kh_init(GridMap);
    kh_resize(GridMap, gm, maxEntities);

    ecs_entity_t newTerrain = ecs_new_entity(world, "SurfaceTerrain");
    ecs_set(world, newTerrain, bc_TerrainMap, {cm, gm});
    ecs_add(world, newTerrain, Surface);

    return newTerrain;
}

void bc_generateTerrain(ecs_world_t *world, ecs_entity_t terrain, u32 seed) {
    htw_ChunkMap *cm = ecs_get(world, terrain, bc_TerrainMap)->chunkMap;
    u32 width = bc_chunkSize;
    u32 height = bc_chunkSize;
    u32 cellsPerChunk = width * height;

    u32 mountainsPerCell = 4;
    bc_seedMountains(cm, mountainsPerCell * cm->chunkCountX * cm->chunkCountY, 128, 64);
    //bc_growMountains(cm, 0.5);

    for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
        for (int x = 0; x < cm->chunkCountX; x++, c++) {
            bc_CellData *cellData = cm->chunks[c].cellData;

            for (int i = 0; i < cellsPerChunk; i++) {
                bc_CellData *cell = &cellData[i];
                htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, c, i);
                float baseNoise = htw_geo_simplex(cm, cellCoord, seed, 8, 16);
                float nutrientNoise = htw_geo_simplex(cm, cellCoord, seed + 1, 4, 16);
                float rainNoise = htw_geo_simplex(cm, cellCoord, seed + 2, 4, 4);
                s32 grad = remap_int(cellCoord.y, 0, cm->mapHeight, 0, 255);
                s32 poleGrad1 = htw_geo_circularGradientByGridCoord(
                    cm, cellCoord, (htw_geo_GridCoord){0, 0}, 255, 0, cm->mapWidth * 0.67);
                s32 poleGrad2 = htw_geo_circularGradientByGridCoord(
                    cm, cellCoord, (htw_geo_GridCoord){0, cm->mapHeight / 2}, 255, 0, cm->mapWidth * 0.33);
                s32 poleGrad3 = htw_geo_circularGradientByGridCoord(
                    cm, cellCoord, (htw_geo_GridCoord){cm->mapWidth / 2, 0}, 255, 0, cm->mapWidth * 0.33);
                s32 poleGrad4 = htw_geo_circularGradientByGridCoord(
                    cm, cellCoord, (htw_geo_GridCoord){cm->mapWidth / 2, cm->mapHeight / 2}, 255, 0, cm->mapWidth * 0.33);
                cell->height += baseNoise * 32;
                cell->temperature = poleGrad1;
                cell->nutrient = nutrientNoise * 32;
                cell->rainfall = rainNoise * 64;
                cell->visibility = 0;
                cell->vegetation = nutrientNoise * 128;
            }
        }
    }
}

void bc_terrainMapAddEntity(ecs_world_t *world, const bc_TerrainMap *tm, ecs_entity_t e, bc_GridPosition pos) {
    khint_t i = kh_get(GridMap, tm->gridMap, pos);
    if (i == kh_end(tm->gridMap)) {
        // No entry here yet, place directly
        int absent;
        i = kh_put(GridMap, tm->gridMap, pos, &absent);
        kh_val(tm->gridMap, i) = e;
    } else {
        // Need to create a new root entity if valid entity is here already
        ecs_entity_t existing = kh_val(tm->gridMap, i);
        if (ecs_is_valid(world, existing)) {
            if (ecs_has(world, existing, CellRoot)) {
                // Add as new child
                ecs_add_pair(world, e, EcsChildOf, existing);
            } else {
                // Make new cell root, add both as child
                // FIXME: if 2 entities move onto the same cell in one update, 2 cellroots get created
                ecs_defer_suspend(world);
                ecs_entity_t newRoot = ecs_new(world, CellRoot);
                ecs_defer_resume(world);
                //ecs_set_name(world, newRoot, ""); // TODO: use pos to create unique name
                ecs_add_pair(world, existing, EcsChildOf, newRoot);
                ecs_add_pair(world, e, EcsChildOf, newRoot);
                int absent;
                khint_t k = kh_put(GridMap, tm->gridMap, pos, &absent);
                kh_val(tm->gridMap, k) = newRoot;
            }
        } else {
            // Invalid entity here, place directly
            kh_val(tm->gridMap, i) = e;
        }
    }
}

void bc_terrainMapRemoveEntity(ecs_world_t *world, const bc_TerrainMap *tm, ecs_entity_t e, bc_GridPosition pos) {
    khint_t i = kh_get(GridMap, tm->gridMap, pos);
    if (i == kh_end(tm->gridMap)) {
        // entity hasn't been placed on the map yet
    } else {
        ecs_entity_t root = kh_val(tm->gridMap, i);
        if (e == root) { // TODO: is this the best way to compare IDs?
            // e is cell root, there will be no other entities in the cell after moving
            kh_del(GridMap, tm->gridMap, i);
        } else {
            // Remove ChildOf relationship
            ecs_entity_t parent = ecs_get_target(world, e, EcsChildOf, 0);
            if (ecs_is_valid(world, parent)) {
                ecs_remove_pair(world, e, EcsChildOf, parent);
            }
        }
    }
}

void bc_terrainMapMoveEntity(ecs_world_t *world, const bc_TerrainMap *tm, ecs_entity_t e, bc_GridPosition oldPos, bc_GridPosition newPos) {
    bc_terrainMapRemoveEntity(world, tm, e, oldPos);
    bc_terrainMapAddEntity(world, tm, e, newPos);
}

void bc_terrainStep(ecs_iter_t *it) {

}

void bc_terrainSeasonalStep(ecs_iter_t *it) {
    bc_TerrainMap *tm = ecs_field(it, bc_TerrainMap, 1);

    for (int i = 0; i < it->count; i++) {
        htw_ChunkMap *cm = tm[i].chunkMap;
        for (int c = 0, y = 0; y < cm->chunkCountY; y++) {
            for (int x = 0; x < cm->chunkCountX; x++, c++) {
                bc_CellData *cellData = cm->chunks[c].cellData;

                for (int cell = 0; cell < cm->cellsPerChunk; cell++) {
                    updateVegetation(&cellData[cell]);
                }
            }
        }
    }
}

void bc_terrainCleanEmptyRoots(ecs_iter_t *it) {
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

void bc_registerTerrainSystems(ecs_world_t *world) {
    ECS_SYSTEM(world, bc_terrainSeasonalStep, EcsOnUpdate, bc_TerrainMap);
    ECS_SYSTEM(world, bc_terrainCleanEmptyRoots, EcsPostFrame, CellRoot);
}

void updateVegetation(bc_CellData *cellData) {
    if (cellData->nutrient > cellData->vegetation) {
        cellData->vegetation++;
        cellData->nutrient -= cellData->vegetation;
    } else {
        cellData->nutrient += cellData->rainfall;
    }
}
