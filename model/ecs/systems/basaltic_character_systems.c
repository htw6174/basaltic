#include "basaltic_character_systems.h"
#include "basaltic_terrain_systems.h"
#include "basaltic_components.h"
#include "basaltic_character.h"
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "flecs.h"

void characterCreated(ecs_iter_t *it);
void characterMoved(ecs_iter_t *it);
void characterDestroyed(ecs_iter_t *it);

void bc_createCharacters(ecs_world_t *world, ecs_entity_t terrainMap, size_t count) {
    const bc_TerrainMap *tm = ecs_get(world, terrainMap, bc_TerrainMap);
    u32 maxX = tm->chunkMap->mapWidth;
    u32 maxY = tm->chunkMap->mapHeight;
    for (size_t i = 0; i < count; i++) {
        ecs_entity_t newCharacter = ecs_new(world, bc_GridPosition);
        htw_geo_GridCoord coord = {
            .x = htw_randRange(maxX),
            .y = htw_randRange(maxY)
        };
        ecs_add_pair(world, newCharacter, IsOn, terrainMap);
        bc_terrainMapAddEntity(world, tm, newCharacter, coord);
        // TODO: figure out if it is necessary to assign each struct field individually when using ecs_set. The 2 lines below should be equivalent, but compiler doesn't like me using coord directly with ecs_set
        //ecs_set_id(world, newCharacter, ecs_id(bc_GridPosition), sizeof(bc_GridPosition), &coord);
        //ecs_set(world, newCharacter, bc_GridPosition, coord);
        ecs_set(world, newCharacter, bc_GridPosition, {coord.x, coord.y});
        ecs_set(world, newCharacter, bc_GridDestination, {coord.x, coord.y});
        ecs_add(world, newCharacter, BehaviorWander);
        // TEST
        if (i % 32 == 0) {
            ecs_add(world, newCharacter, PlayerVision);
        }
    }
}

void bc_setCharacterDestination(ecs_world_t *world, ecs_entity_t e, htw_geo_GridCoord dest) {
    // TODO: movement range checking
    // TODO: stamina use vs current stamina checking
    // TODO: return should indicate if destination is valid/can be set
    if (ecs_has(world, e, bc_GridDestination)) {
        ecs_set(world, e, bc_GridDestination, {dest.x, dest.y});
    }
}

void bc_setWandererDestinations(ecs_iter_t *it) {
    bc_GridPosition *positions = ecs_field(it, bc_GridPosition, 1);
    bc_GridDestination *destinations = ecs_field(it, bc_GridDestination, 2);
    ecs_entity_t tmEnt = ecs_field_id(it, 3);
    const bc_TerrainMap *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), bc_TerrainMap);

    for (int i = 0; i < it->count; i++) {
        destinations[i] = htw_geo_addGridCoordsWrapped(tm->chunkMap, positions[i], htw_geo_hexGridDirections[htw_randRange(HEX_DIRECTION_COUNT)]);
    }
}

void bc_setDescenderDestinations(ecs_iter_t *it) {
    bc_GridPosition *positions = ecs_field(it, bc_GridPosition, 1);
    bc_GridDestination *destinations = ecs_field(it, bc_GridDestination, 2);
    ecs_entity_t tmEnt = ecs_field_id(it, 3);
    const bc_TerrainMap *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), bc_TerrainMap);

    for (int i = 0; i < it->count; i++) {
        s32 lowestDirection = -1;
        s32 lowestElevation = ((bc_CellData*)htw_geo_getCell(tm->chunkMap, positions[i]))->height;
        for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
            // Get cell elevation
            htw_geo_GridCoord evalCoord = htw_geo_addGridCoordsWrapped(tm->chunkMap, positions[i], htw_geo_hexGridDirections[d]);
            bc_CellData *cell = (bc_CellData*)htw_geo_getCell(tm->chunkMap, evalCoord);
            if (cell->height < lowestElevation) lowestDirection = d;
            lowestElevation = min_int(lowestElevation, cell->height);
        }
        if (lowestDirection == -1) {
            continue;
        } else {
            destinations[i] = htw_geo_addGridCoordsWrapped(tm->chunkMap, positions[i], htw_geo_hexGridDirections[lowestDirection]);
        }
    }
}

// TODO: tell flecs to process movement without queuing commands
void bc_moveCharacters(ecs_iter_t *it) {
    // TODO: wrap position according to chunk map
    // TODO: should I wrap when setting destinations too?
    bc_GridPosition *positions = ecs_field(it, bc_GridPosition, 1);
    bc_GridDestination *destinations = ecs_field(it, bc_GridDestination, 2);
    ecs_entity_t tmEnt = ecs_field_id(it, 3);
    const bc_TerrainMap *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), bc_TerrainMap);

    for (int i = 0; i < it->count; i++) {
        u32 movementDistance = htw_geo_hexGridDistance(positions[i], destinations[i]);
        // TODO: move towards destination by maximum single turn move distance
        bc_terrainMapMoveEntity(it->world, tm, it->entities[i], positions[i], destinations[i]);
        // TODO: if entity has stamina, deduct stamina (or add if no move taken. Should maybe be in a seperate system)
        positions[i] = destinations[i];
    }
}

void bc_revealMap(ecs_iter_t *it) {
    bc_GridPosition *positions = ecs_field(it, bc_GridPosition, 1);
    ecs_entity_t tmEnt = ecs_field_id(it, 2);
    const bc_TerrainMap *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), bc_TerrainMap);
    htw_ChunkMap *cm = tm->chunkMap;

    for (int i = 0; i < it->count; i++) {
        // TODO: factor in terrain height, character size, and attributes e.g. Flying
        // get character's current cell information
        u32 charChunkIndex, charCellIndex;
        htw_geo_gridCoordinateToChunkAndCellIndex(cm, positions[i], &charChunkIndex, &charCellIndex);
        s32 characterElevation = bc_getCellByIndex(cm, charChunkIndex, charCellIndex)->height;

        htw_geo_CubeCoord charCubeCoord = htw_geo_gridToCubeCoord(positions[i]);
        htw_geo_CubeCoord relativeCoord = {0, 0, 0};

        // get number of cells to check based on character's attributes
        u32 sightRange = 3;
        u32 detailRange = sightRange - 1;
        u32 cellsInSightRange = htw_geo_getHexArea(sightRange);
        u32 cellsInDetailRange = htw_geo_getHexArea(detailRange);

        // TODO: because of the outward spiral cell iteration used here, it may be possible to keep track of cell attributes that affect visibility and apply them additively to more distant cells (would probably only make sense to do this in the same direction; would have to come up with a way to determine if a cell is 'behind' another), such as forests or high elevations blocking lower areas
        for (int c = 0; c < cellsInSightRange; c++) {
            // Result coordinate is not confined to chunkmap, but converting to chunk and cell will also wrap input coords
            htw_geo_CubeCoord worldCubeCoord = htw_geo_addCubeCoords(charCubeCoord, relativeCoord);
            htw_geo_GridCoord worldCoord = htw_geo_cubeToGridCoord(worldCubeCoord);
            u32 chunkIndex, cellIndex;
            htw_geo_gridCoordinateToChunkAndCellIndex(cm, worldCoord, &chunkIndex, &cellIndex);
            bc_CellData *cell = bc_getCellByIndex(cm, chunkIndex, cellIndex);

            bc_TerrainVisibilityBitFlags cellVisibility = 0;
            // restrict sight by distance
            // NOTE: this only works because of the outward spiral iteration pattern. Should use cube coordinate distance instead for better reliability
            if (c < cellsInDetailRange) {
                cellVisibility = BC_TERRAIN_VISIBILITY_GEOMETRY | BC_TERRAIN_VISIBILITY_COLOR;
            } else {
                cellVisibility = BC_TERRAIN_VISIBILITY_GEOMETRY;
            }
            // restrict sight by relative elevation
            s32 elevation = cell->height;
            if (elevation > characterElevation + 1) { // TODO: instead of constant +1, derive from character attributes
                cellVisibility = BC_TERRAIN_VISIBILITY_GEOMETRY;
            }

            cell->visibility |= cellVisibility;

            htw_geo_getNextHexSpiralCoord(&relativeCoord);
        }
    }
}

void bc_registerCharacterSystems(ecs_world_t *world) {
    ECS_SYSTEM(world, bc_setWandererDestinations, EcsPreUpdate, [in] bc_GridPosition, [out] bc_GridDestination, [in] (IsOn, _), BehaviorWander, !PlayerControlled);
    ECS_SYSTEM(world, bc_setDescenderDestinations, EcsPreUpdate, [in] bc_GridPosition, [out] bc_GridDestination, [in] (IsOn, _), BehaviorDescend, !PlayerControlled);
    ECS_SYSTEM(world, bc_moveCharacters, EcsOnUpdate, [out] bc_GridPosition, [in] bc_GridDestination, [in] (IsOn, _));
    ECS_SYSTEM(world, bc_revealMap, EcsPostUpdate, [in] bc_GridPosition, [in] (IsOn, _), PlayerVision);
    //ECS_OBSERVER(world, characterMoved, EcsOnSet, bc_GridPosition, (IsOn, _));
    ECS_OBSERVER(world, characterDestroyed, EcsOnDelete, bc_GridPosition, (IsOn, _));
}

void characterCreated(ecs_iter_t *it) {

}

void characterMoved(ecs_iter_t *it) {
    bc_GridPosition *positions = ecs_field(it, bc_GridPosition, 1);
    ecs_entity_t tmEnt = ecs_field_id(it, 2);
    const bc_TerrainMap *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), bc_TerrainMap);
    for (int i = 0; i < it->count; i++) {
        bc_terrainMapMoveEntity(it->world, tm, it->entities[i], (bc_GridPosition){0, 0}, positions[i]);
    }
}

void characterDestroyed(ecs_iter_t *it) {
    bc_GridPosition *positions = ecs_field(it, bc_GridPosition, 1);
    ecs_entity_t tmEnt = ecs_field_id(it, 2);
    const bc_TerrainMap *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), bc_TerrainMap);
    for (int i = 0; i < it->count; i++) {
        bc_terrainMapRemoveEntity(it->world, tm, it->entities[i], positions[i]);
    }
}
