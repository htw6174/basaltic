#include "basaltic_character_systems.h"
#include "basaltic_components.h"
#include "components/basaltic_components_planes.h"
#include "components/basaltic_components_actors.h"
#include "basaltic_character.h"
#include "basaltic_worldState.h"
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "flecs.h"

ECS_TAG_DECLARE(BehaviorWander);
ECS_TAG_DECLARE(BehaviorDescend);
ECS_TAG_DECLARE(BehaviorGrazer);
ECS_TAG_DECLARE(BehaviorPredator);

// TEST: random movement behavior, pick any adjacent tile to move to
void bc_setWandererDestinations(ecs_iter_t *it);
// TEST: check neighboring cell features, move towards lowest elevation
void bc_setDescenderDestinations(ecs_iter_t *it);
void bc_moveCharacters(ecs_iter_t *it);
void bc_revealMap(ecs_iter_t *it);

void characterCreated(ecs_iter_t *it);
void characterMoved(ecs_iter_t *it);
void characterDestroyed(ecs_iter_t *it);

// TEST: simple ecosystem behaviors
void behaviorGraze(ecs_iter_t *it);
void behaviorPredate(ecs_iter_t *it);

// TODO: should consider this to be a test function. Can repurpose some of the logic, but populating the world should be a more specialized task. Maybe build off of entities/systems so that the editor has access to spawning variables and functions
void bc_createCharacters(ecs_world_t *world, ecs_entity_t plane, size_t count) {
    ecs_defer_begin(world);
    const Plane *tm = ecs_get(world, plane, Plane);
    u32 maxX = tm->chunkMap->mapWidth;
    u32 maxY = tm->chunkMap->mapHeight;
    for (size_t i = 0; i < count; i++) {
        ecs_entity_t newCharacter = ecs_new(world, Position);
        htw_geo_GridCoord coord = {
            .x = htw_randRange(maxX),
            .y = htw_randRange(maxY)
        };
        ecs_add_pair(world, newCharacter, IsOn, plane);
        plane_PlaceEntity(world, plane, newCharacter, coord);
        // TODO: figure out if it is necessary to assign each struct field individually when using ecs_set. The 2 lines below should be equivalent, but compiler doesn't like me using coord directly with ecs_set
        //ecs_set_id(world, newCharacter, ecs_id(Position), sizeof(Position), &coord);
        //ecs_set(world, newCharacter, Position, coord);
        ecs_set(world, newCharacter, Position, {coord.x, coord.y});
        ecs_set(world, newCharacter, Destination, {coord.x, coord.y});
        // TEST
        if (i % 32 == 0) {
            ecs_add(world, newCharacter, BehaviorPredator);
            ecs_add(world, newCharacter, PlayerVision);
        } else if (i % 4 == 0) {
            ecs_add(world, newCharacter, BehaviorGrazer);
        } else {
            ecs_add(world, newCharacter, BehaviorGrazer);
        }
    }
    ecs_defer_end(world);
}

void bc_setCharacterDestination(ecs_world_t *world, ecs_entity_t e, htw_geo_GridCoord dest) {
    // TODO: movement range checking
    // TODO: stamina use vs current stamina checking
    // TODO: return should indicate if destination is valid/can be set
    // NOTE: instead of having a simple function for something that can be easily handled with the reflection framework, make the TODO checks above into their own functions that the View can use, as well as systems that control character movement
    if (ecs_has(world, e, Destination)) {
        ecs_set(world, e, Destination, {dest.x, dest.y});
    }
}

void SurfaceSpawn(ecs_iter_t *it) {
    Plane *planes = ecs_field(it, Plane, 1);

    for (int i = 0; i < it->count; i++) {
        bc_createCharacters(it->world, it->entities[i], 1000);
    }
}

void bc_setWandererDestinations(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    ecs_entity_t tmEnt = ecs_field_id(it, 3);
    const Plane *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), Plane);

    for (int i = 0; i < it->count; i++) {
        destinations[i] = htw_geo_addGridCoordsWrapped(tm->chunkMap, positions[i], htw_geo_hexGridDirections[htw_randRange(HEX_DIRECTION_COUNT)]);
    }
}

void bc_setDescenderDestinations(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    ecs_entity_t tmEnt = ecs_field_id(it, 3);
    const Plane *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), Plane);

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
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    ecs_entity_t tmEnt = ecs_field_id(it, 3);
    const Plane *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), Plane);

    for (int i = 0; i < it->count; i++) {
        u32 movementDistance = htw_geo_hexGridDistance(positions[i], destinations[i]);
        // TODO: move towards destination by maximum single turn move distance
        plane_MoveEntity(it->world, tmEnt, it->entities[i], positions[i], destinations[i]);
        // TODO: if entity has stamina, deduct stamina (or add if no move taken. Should maybe be in a seperate system)
        positions[i] = destinations[i];
    }
}

void bc_revealMap(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    ecs_entity_t tmEnt = ecs_field_id(it, 2);
    const Plane *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), Plane);
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

void BasalticSystemsCharactersImport(ecs_world_t *world) {
    ECS_MODULE(world, BasalticSystemsCharacters);

    ECS_IMPORT(world, BasalticComponents);

    ECS_TAG_DEFINE(world, BehaviorWander);
    ECS_TAG_DEFINE(world, BehaviorDescend);
    ECS_TAG_DEFINE(world, BehaviorGrazer);
    ECS_TAG_DEFINE(world, BehaviorPredator);

    // TODO: replace behavior tags with Ego relationship
    ECS_SYSTEM(world, bc_setWandererDestinations, EcsPreUpdate,
        [in] Position,
        [out] Destination,
        [in] (basaltic.components.planes.IsOn, _),
        [none] BehaviorWander,
    );

    ECS_SYSTEM(world, bc_setDescenderDestinations, EcsPreUpdate,
        [in] Position,
        [out] Destination,
        [in] (basaltic.components.planes.IsOn, _),
        [none] BehaviorDescend,
    );

    //ECS_SYSTEM(world, bc_moveCharacters, EcsOnUpdate, [out] Position, [in] Destination, [in] (IsOn, _));
    ecs_entity_t ecs_id(bc_moveCharacters) = ecs_system(world, {
        .entity = ecs_entity(world, {
            .name = "bc_moveCharacters",
            .add = { ecs_dependson(EcsOnUpdate) }
        }),
        .query.filter.terms = {
            { .id = ecs_id(Position), .inout = EcsOut },
            { .id = ecs_id(Destination), .inout = EcsIn },
            { .id = ecs_pair(IsOn, EcsAny), .inout = EcsIn },
        },
        .callback = bc_moveCharacters,
        .no_readonly = true // disable readonly mode for this system
    });
    ECS_SYSTEM(world, bc_revealMap, EcsPostUpdate,
        [in] Position,
        [in] (basaltic.components.planes.IsOn, _),
        [none] basaltic.components.PlayerVision
    );
    //ECS_OBSERVER(world, characterMoved, EcsOnSet, Position, (IsOn, _));
    ECS_OBSERVER(world, characterDestroyed, EcsOnDelete, Position, (basaltic.components.planes.IsOn, _));

    // TEST ecosystem behaviors
    ECS_SYSTEM(world, behaviorGraze, EcsPreUpdate,
        [in] Position,
        [out] Destination,
        [in] (basaltic.components.planes.IsOn, _),
        BehaviorGrazer
    );
    ECS_SYSTEM(world, behaviorPredate, EcsPreUpdate,
        [in] Position,
        [out] Destination,
        [in] (basaltic.components.planes.IsOn, _),
        BehaviorPredator
    );

    ECS_SYSTEM(world, SurfaceSpawn, EcsOnStart,
        [in] Plane
    );

    // NOTE: by passing an existing system to .entity, properties of the existing system can be overwritten. Useful when creating systems with ECS_SYSTEM that need finer control than the macro provides.
    ecs_system(world, {
        .entity = SurfaceSpawn,
        .no_readonly = true
    });
}

void characterCreated(ecs_iter_t *it) {

}

void characterMoved(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    ecs_entity_t tmEnt = ecs_field_id(it, 2);
    for (int i = 0; i < it->count; i++) {
        plane_MoveEntity(it->world, tmEnt, it->entities[i], (Position){0, 0}, positions[i]);
    }
}

void characterDestroyed(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    ecs_entity_t tmEnt = ecs_field_id(it, 2);
    for (int i = 0; i < it->count; i++) {
        plane_RemoveEntity(it->world, tmEnt, it->entities[i], positions[i]);
    }
}

void behaviorGraze(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    ecs_entity_t tmEnt = ecs_field_id(it, 3);
    const Plane *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), Plane);
    htw_ChunkMap *cm = tm->chunkMap;

    for (int i = 0; i < it->count; i++) {

        htw_geo_CubeCoord charCubeCoord = htw_geo_gridToCubeCoord(positions[i]);
        htw_geo_CubeCoord relativeCoord = {0, 0, 0};

        // get number of cells to check based on character's attributes
        u32 sightRange = 2;
        u32 cellsInSightRange = htw_geo_getHexArea(sightRange);

        // track best candidate
        s32 bestVegetation = 0;
        htw_geo_CubeCoord bestDirection = {0, 0, 0};

        for (int c = 0; c < cellsInSightRange; c++) {
            // Result coordinate is not confined to chunkmap, but converting to chunk and cell will also wrap input coords
            htw_geo_CubeCoord worldCubeCoord = htw_geo_addCubeCoords(charCubeCoord, relativeCoord);
            htw_geo_GridCoord worldCoord = htw_geo_cubeToGridCoord(worldCubeCoord);
            u32 chunkIndex, cellIndex;
            htw_geo_gridCoordinateToChunkAndCellIndex(cm, worldCoord, &chunkIndex, &cellIndex);
            bc_CellData *cell = bc_getCellByIndex(cm, chunkIndex, cellIndex);

            // Consume some of current cell vegetation
            if (c == 0) {
                cell->vegetation -= cell->vegetation * 0.1;
                if (cell->vegetation > 40) {
                    break;
                }
            }

            // Get cell data, find best vegetation
            if (cell->vegetation > bestVegetation) {
                bestVegetation = cell->vegetation;
                bestDirection = relativeCoord;
            }

            // TODO: check for predators, move in opposite direction and break if found

            htw_geo_getNextHexSpiralCoord(&relativeCoord);
        }

        // Apply best direction found
        destinations[i] = htw_geo_addGridCoordsWrapped(cm, positions[i], htw_geo_cubeToGridCoord(bestDirection));
    }
}

void behaviorPredate(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    ecs_entity_t tmEnt = ecs_field_id(it, 3);
    htw_ChunkMap *cm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), Plane)->chunkMap;

    for (int i = 0; i < it->count; i++) {

        htw_geo_CubeCoord charCubeCoord = htw_geo_gridToCubeCoord(positions[i]);
        htw_geo_CubeCoord relativeCoord = {0, 0, 0};

        // get number of cells to check based on character's attributes
        u32 sightRange = 3;
        u32 cellsInSightRange = htw_geo_getHexArea(sightRange);

        bool inPersuit = false;

        for (int c = 0; c < cellsInSightRange; c++) {
            htw_geo_CubeCoord worldCubeCoord = htw_geo_addCubeCoords(charCubeCoord, relativeCoord);
            htw_geo_GridCoord worldCoord = htw_geo_cubeToGridCoord(worldCubeCoord);
            worldCoord = htw_geo_wrapGridCoordOnChunkMap(cm, worldCoord);

            // Get root entity at cell, find any Grazers, break if found
            // TODO: Descend tree. for now, only pick out lone grazers (stragglers)
            ecs_entity_t root = plane_GetRootEntity(it->world, tmEnt, worldCoord);
            if (ecs_is_valid(it->world, root)) {
                if (ecs_has(it->world, root, BehaviorGrazer)) {
                    // Apply destination
                    destinations[i] = worldCoord;
                    inPersuit = true;
                    break;
                }
            }

            htw_geo_getNextHexSpiralCoord(&relativeCoord);
        }
        if (!inPersuit) {
            // move randomly
            destinations[i] = htw_geo_addGridCoordsWrapped(cm, positions[i], htw_geo_hexGridDirections[htw_randRange(HEX_DIRECTION_COUNT)]);
        }
    }
}
