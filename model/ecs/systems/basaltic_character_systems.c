#include "basaltic_character_systems.h"
#include "basaltic_components.h"
#include "basaltic_phases.h"
#include "components/basaltic_components_planes.h"
#include "components/basaltic_components_actors.h"
#include "components/basaltic_components_wildlife.h"
#include "basaltic_character.h"
#include "basaltic_worldState.h"
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "flecs.h"

// TEST: random movement behavior, pick any adjacent tile to move to
void bc_setWandererDestinations(ecs_iter_t *it);
// TEST: check neighboring cell features, move towards lowest elevation
void bc_setDescenderDestinations(ecs_iter_t *it);
void bc_revealMap(ecs_iter_t *it);

void characterCreated(ecs_iter_t *it);
void characterMoved(ecs_iter_t *it);
void characterDestroyed(ecs_iter_t *it);

void spawnActors(ecs_iter_t *it);

void egoBehaviorWander(ecs_iter_t *it);
void egoBehaviorGrazer(ecs_iter_t *it);
void egoBehaviorPredator(ecs_iter_t *it);

void executeMove(ecs_iter_t *it);
void executeFeed(ecs_iter_t *it);

void resolveHealth(ecs_iter_t *it);

void mergeGroups(ecs_iter_t *it);

void tickGrowth(ecs_iter_t *it);
void tickStamina(ecs_iter_t *it);

// TODO: should consider this to be a test function. Can repurpose some of the logic, but populating the world should be a more specialized task. Maybe build off of entities/systems so that the editor has access to spawning variables and functions
void bc_createCharacters(ecs_world_t *world, ecs_entity_t plane, size_t count) {
    ecs_defer_begin(world);
    ecs_set_scope(world, plane);
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
            ecs_add_pair(world, newCharacter, Ego, EgoPredator);
            ecs_add_pair(world, newCharacter, Diet, Meat);
            ecs_add(world, newCharacter, PlayerVision);
        } else if (i % 8 == 0) {
            ecs_add_pair(world, newCharacter, Ego, EgoGrazer);
            ecs_add_pair(world, newCharacter, Diet, Fruit);
        } else if (i % 4 == 0) {
            ecs_add_pair(world, newCharacter, Ego, EgoGrazer);
            ecs_add_pair(world, newCharacter, Diet, Foliage);
        } else {
            ecs_add_pair(world, newCharacter, Ego, EgoGrazer);
            ecs_add_pair(world, newCharacter, Diet, Grasses);
        }
    }
    ecs_set_scope(world, 0);
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

void egoBehaviorWander(ecs_iter_t *it) {
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
        s32 lowestElevation = ((CellData*)htw_geo_getCell(tm->chunkMap, positions[i]))->height;
        for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
            // Get cell elevation
            htw_geo_GridCoord evalCoord = htw_geo_addGridCoordsWrapped(tm->chunkMap, positions[i], htw_geo_hexGridDirections[d]);
            CellData *cell = (CellData*)htw_geo_getCell(tm->chunkMap, evalCoord);
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
            CellData *cell = bc_getCellByIndex(cm, chunkIndex, cellIndex);

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

void characterCreated(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    ecs_entity_t tmEnt = ecs_field_id(it, 2);
    for (int i = 0; i < it->count; i++) {
        plane_PlaceEntity(it->world, tmEnt, it->entities[i], positions[i]);
    }
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

void spawnActors(ecs_iter_t *it) {
    Spawner *spawners = ecs_field(it, Spawner, 1);
    ecs_entity_t plane = ecs_field_id(it, 2);

    ecs_world_t *world = it->world;
    htw_ChunkMap *cm = ecs_get(world, ecs_pair_second(world, plane), Plane)->chunkMap;

    ecs_entity_t oldScope = ecs_get_scope(world);
    ecs_set_scope(world, plane);

    u32 maxX = cm->mapWidth;
    u32 maxY = cm->mapHeight;

    ecs_defer_begin(world);
    for (int i = 0; i < it->count; i++) {
        Spawner sp = spawners[i];
        for (int e = 0; e < sp.count; e++) {
            ecs_entity_t newCharacter = ecs_new(it->world, 0);
            htw_geo_GridCoord coord = {
                .x = htw_randRange(maxX),
                .y = htw_randRange(maxY)
            };
            ecs_add_pair(world, newCharacter, IsOn, plane);
            plane_PlaceEntity(world, plane, newCharacter, coord);
            ecs_set(world, newCharacter, Position, {coord.x, coord.y});
            ecs_set(world, newCharacter, Destination, {coord.x, coord.y});
            ecs_add_pair(world, newCharacter, EcsIsA, sp.prefab);
        }
        if (sp.oneShot) {
            ecs_delete(world, it->entities[i]);
        }
    }
    ecs_defer_end(world);

    ecs_set_scope(world, oldScope);
}

void egoBehaviorGrazer(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    ecs_entity_t tmEnt = ecs_field_id(it, 3);

    const Plane *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), Plane);
    htw_ChunkMap *cm = tm->chunkMap;

    for (int i = 0; i < it->count; i++) {

        htw_geo_CubeCoord charCubeCoord = htw_geo_gridToCubeCoord(positions[i]);
        htw_geo_CubeCoord relativeCoord = {0, 0, 0};

        // track best candidate
        CellData *cell = htw_geo_getCell(cm, positions[i]);
        u32 availableHere = cell->understory;

        // if there is enough here, stay put and feed. Otherwise, find best nearby cell
        // TODO: first check for and move directly away from predators
        u32 tolerance = 10;
        if (availableHere >= tolerance) {
            ecs_add_pair(it->world, it->entities[i], Action, ActionFeed);
        } else {
            // get number of cells to check based on character's attributes
            u32 sightRange = 2;
            u32 cellsInSightRange = htw_geo_getHexArea(sightRange);
            u32 bestAvailable = availableHere;
            htw_geo_CubeCoord bestDirection = {0, 0, 0};
            for (int c = 0; c < cellsInSightRange; c++) {
                // Result coordinate is not confined to chunkmap, but converting to chunk and cell will also wrap input coords
                htw_geo_CubeCoord worldCubeCoord = htw_geo_addCubeCoords(charCubeCoord, relativeCoord);
                htw_geo_GridCoord worldCoord = htw_geo_cubeToGridCoord(worldCubeCoord);
                u32 chunkIndex, cellIndex;
                htw_geo_gridCoordinateToChunkAndCellIndex(cm, worldCoord, &chunkIndex, &cellIndex);
                cell = bc_getCellByIndex(cm, chunkIndex, cellIndex);

                // Get cell data, find best vegetation
                if (cell->understory > bestAvailable) {
                    bestAvailable = cell->understory;
                    bestDirection = relativeCoord;
                }

                htw_geo_getNextHexSpiralCoord(&relativeCoord);
            }
            // if no cell with more than scraps, move randomly
            if (bestAvailable < tolerance) {
                s32 dir = htw_randRange(HEX_DIRECTION_COUNT);
                bestDirection = htw_geo_cubeDirections[dir];
            }

            // Apply best direction found
            destinations[i] = htw_geo_addGridCoordsWrapped(cm, positions[i], htw_geo_cubeToGridCoord(bestDirection));
            ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
        }
    }
}

void egoBehaviorPredator(ecs_iter_t *it) {
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
                if (ecs_has_pair(it->world, root, Ego, EgoGrazer)) {
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
        ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
    }
}

void executeMove(ecs_iter_t *it) {
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

void executeFeed(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    ecs_entity_t tmEnt = ecs_field_id(it, 2);
    const Plane *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), Plane);
    htw_ChunkMap *cm = tm->chunkMap;
    ecs_entity_t diet = ecs_pair_second(it->world, ecs_field_id(it, 3));
    Condition *conditions = ecs_field(it, Condition, 4);
    if (ecs_field_is_set(it, 5)) {
        // multiply consumed amount by group size
        Group *groups = ecs_field(it, Group, 5);
        for (int i = 0; i < it->count; i++) {
            CellData *cell = htw_geo_getCell(cm, positions[i]);
            s32 required = groups[i].count; // TODO: multiply by size?
            s32 consumed = min_int(cell->understory, required);
            cell->understory -= consumed;
            // Restore up to half of max each meal TODO best rounding method?
            s32 restored = roundf( ((float)conditions[i].maxStamina / 2) * ((float)consumed / required) );
            conditions[i].stamina = min_int(conditions[i].stamina + restored, conditions[i].maxStamina);
        }
    } else {
        for (int i = 0; i < it->count; i++) {
            CellData *cell = htw_geo_getCell(cm, positions[i]);
            s32 consumed = min_int(cell->understory, 1);
            cell->understory -= consumed;
            s32 restored = conditions[i].maxStamina / 2; // Restore half of max each meal
            conditions[i].stamina = min_int(conditions[i].stamina + restored, conditions[i].maxStamina);
        }
    }
}

void resolveHealth(ecs_iter_t *it) {
    Condition *conditions = ecs_field(it, Condition, 1);
    Group *groups = ecs_field(it, Group, 2);

    for (int i = 0; i < it->count; i++) {
        // apply starvation
        if (conditions[i].stamina <= -conditions[i].maxStamina) {
            conditions[i].health -= groups[i].count;
        } else if (conditions[i].stamina > 0) {
            // heal if not malnourished
            conditions[i].health = min_int(conditions[i].health + 1, conditions[i].maxHealth);
        }

        // handle group member death
        // NOTE: 2x2 options: reduce group count according to how far in the red health is, or only kill one at a time; reset health to max, or increasae by max so that damage can 'overflow'
        // for now, try simplest option: reduce by 1 and reset to max
        if (conditions[i].health <= 0) {
            conditions[i].health = conditions[i].maxHealth;
            groups[i].count -= 1;
            // destroy empty group
            if (groups[i].count <= 0) {
                ecs_delete(it->world, it->entities[i]);
            }
        }
    }
}

void mergeGroups(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    ecs_entity_t tmEnt = ecs_field_id(it, 2);
    const Plane *tm = ecs_get(it->world, ecs_pair_second(it->world, tmEnt), Plane);
    htw_ChunkMap *cm = tm->chunkMap;
    Group *groups = ecs_field(it, Group, 3);
    ecs_entity_t prefab = ecs_pair_second(it->world, ecs_field_id(it, 4));

    for (int i = 0; i < it->count; i++) {

        ecs_entity_t root = plane_GetRootEntity(it->world, tmEnt, positions[i]);
        // filter for: entities in root's hierarchy tree & instances of the same prefab & is a group
        ecs_filter_t *filter = ecs_filter(it->world, {
            .terms = {
                {ecs_id(Group)},
                {ecs_pair(EcsIsA, prefab)},
                {ecs_pair(EcsChildOf, root)}
            }
        });
        ecs_iter_t fit = ecs_filter_iter(it->world, filter);
        while (ecs_filter_next(&fit)) {
            Group *otherGroups = ecs_field(&fit, Group, 1);

            for (int f = 0; f < fit.count; f++) {
                // TODO better rule later; for now only merge with groups at least 10x the size
                if (otherGroups[i].count >= groups[i].count * 8) {
                    // merge into other group and delete original TODO: setup test scenario, doesn't occour naturally very often
                    otherGroups[i].count += groups[i].count;
                    ecs_defer_begin(it->world);
                    ecs_delete(it->world, it->entities[i]);
                    ecs_defer_end(it->world);
                }
            }
        }
        ecs_filter_fini(filter);
    }
}

void tickGrowth(ecs_iter_t *it) {
    GrowthRate *growthRates = ecs_field(it, GrowthRate, 1);
    Group *groups = ecs_field(it, Group, 2);

    for (int i = 0; i < it->count; i++) {
        growthRates[i].progress += groups[i].count / 2;
        if (growthRates[i].progress >= growthRates[i].stepsRequired) {
            growthRates[i].progress = 0;
            groups[i].count += 1;
        }
    }
}

void tickStamina(ecs_iter_t *it) {
    Condition *conditions = ecs_field(it, Condition, 1);

    for (int i = 0; i < it->count; i++) {
        s32 maxStam = conditions[i].maxStamina;
        s32 newStam = conditions[i].stamina - 1;
        conditions[i].stamina = max_int(-maxStam, newStam);
    }
}

void BcSystemsCharactersImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystemsCharacters);

    // ECS_IMPORT(world, Bc); // NOTE: this import does nothing, because `Bc` is a 'parent' path of `BcSystemsCharacters`
    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);
    ECS_IMPORT(world, BcActors);
    ECS_IMPORT(world, BcWildlife);

    // TODO: change IsOn field, use Plane(up(bc.planes.IsOn)) instead
    // - NOTE: should ask about or profile this; could cause the query to switch to per-instance iteration instead, which may negatively effect performance. Tradeoff is that access to the Plane is faster?
    ECS_SYSTEM(world, egoBehaviorWander, Planning,
               [in] Position,
               [out] Destination,
               [in] (bc.planes.IsOn, _),
               [none] (bc.actors.Ego, bc.actors.Ego.EgoWanderer)
    );

    ECS_SYSTEM(world, bc_revealMap, Resolution,
               [in] Position,
               [in] (bc.planes.IsOn, _),
               [none] bc.PlayerVision
    );
    //ECS_OBSERVER(world, characterMoved, EcsOnSet, Position, (IsOn, _));
    ECS_OBSERVER(world, characterDestroyed, EcsOnDelete, Position, (bc.planes.IsOn, _));

    ECS_SYSTEM(world, spawnActors, EcsPreUpdate,
        [in] Spawner,
        [in] (bc.planes.IsOn, _)
    );
    // NOTE: by passing an existing system to .entity, properties of the existing system can be overwritten. Useful when creating systems with ECS_SYSTEM that need finer control than the macro provides.
    ecs_system(world, {
        .entity = spawnActors,
        .no_readonly = true
    });

    // wildlife behaviors
    // NOTE: would like to have a better way to access tags, paths are still getting verbose; for OneOf relationships, need to remember that relationship path is always start of tag fullpath
    ECS_SYSTEM(world, egoBehaviorGrazer, Planning,
               [in] Position,
               [out] Destination,
               [in] (bc.planes.IsOn, _),
               [none] (bc.actors.Ego, bc.actors.Ego.EgoGrazer),
    );
    ecs_system(world, {
        .entity = egoBehaviorGrazer,
        .multi_threaded = true
    });

    ECS_SYSTEM(world, egoBehaviorPredator, Planning,
               [in] Position,
               [out] Destination,
               [in] (bc.planes.IsOn, _),
               [none] (bc.actors.Ego, bc.actors.Ego.EgoPredator),
    );
    ecs_system(world, {
        .entity = egoBehaviorPredator,
        .multi_threaded = true
    });

    ECS_SYSTEM(world, executeMove, Execution,
               [out] Position,
               [in] Destination,
               [in] (bc.planes.IsOn, _),
               [none] (bc.actors.Action, bc.actors.Action.ActionMove)
    );
    // movement actions must be synchronious TODO: revisit this; could be enough to have a cleanup step to enforce the 1 root per cell rule
    ecs_system(world, {
        .entity = executeMove,
        .no_readonly = true
    });

    ECS_SYSTEM(world, executeFeed, Execution,
               [in] Position,
               [in] (bc.planes.IsOn, _),
               [in] (bc.wildlife.Diet, _),
               [in] Condition,
               [in] ?Group,
               [none] (bc.actors.Action, bc.actors.Action.ActionFeed)
    );

    ECS_SYSTEM(world, resolveHealth, Resolution,
               [inout] Condition,
               [inout] Group
    );

    ECS_SYSTEM(world, mergeGroups, Cleanup,
               [in] Position,
               [in] (bc.planes.IsOn, _),
               [inout] Group,
               [in] (IsA, _) // TODO: consider a better way to determine merge compatability. Can't use query variables for cached queries
    );
    ecs_system(world, {
        .entity = mergeGroups,
        .no_readonly = true
    });

    ECS_SYSTEM(world, tickGrowth, AdvanceHour,
               [inout] GrowthRate,
               [inout] Group
    );

    ECS_SYSTEM(world, tickStamina, AdvanceHour,
               [inout] Condition
    );

    /* Prefabs */

    // actor
    ecs_entity_t actorPrefab = ecs_set_name(world, 0, "actorPrefab");
    ecs_add_id(world, actorPrefab, EcsPrefab);
    ecs_override(world, actorPrefab, Position);
    ecs_override(world, actorPrefab, Destination);
    ecs_override(world, actorPrefab, Condition);

    // wolf pack
    ecs_entity_t wolfPackPrefab = ecs_set_name(world, 0, "WolfPackPrefab");
    ecs_add_id(world, wolfPackPrefab, EcsPrefab);
    ecs_add_pair(world, wolfPackPrefab, EcsIsA, actorPrefab);
    ecs_add_pair(world, wolfPackPrefab, Ego, EgoPredator);
    ecs_add_pair(world, wolfPackPrefab, Diet, Meat);
    ecs_set(world, wolfPackPrefab, Condition, {.maxHealth = 30, .health = 30, .maxStamina = 150, .stamina = 150});
    ecs_override(world, wolfPackPrefab, Condition);
    ecs_set(world, wolfPackPrefab, Group, {.count = 10});
    ecs_override(world, wolfPackPrefab, Group);
    ecs_set(world, wolfPackPrefab, GrowthRate, {.stepsRequired = 360 * 24, .progress = 0});
    ecs_override(world, wolfPackPrefab, GrowthRate);
    ecs_set(world, wolfPackPrefab, ActorSize, {ACTOR_SIZE_AVERAGE});

    // bison herd
    ecs_entity_t bisonHerdPrefab = ecs_set_name(world, 0, "BisonHerdPrefab");
    ecs_add_id(world, bisonHerdPrefab, EcsPrefab);
    ecs_add_pair(world, bisonHerdPrefab, EcsIsA, actorPrefab);
    ecs_add_pair(world, bisonHerdPrefab, Ego, EgoGrazer);
    ecs_add_pair(world, bisonHerdPrefab, Diet, Grasses);
    ecs_set(world, bisonHerdPrefab, Condition, {.maxHealth = 100, .health = 100, .maxStamina = 25, .stamina = 25});
    ecs_override(world, bisonHerdPrefab, Condition);
    ecs_set(world, bisonHerdPrefab, Group, {.count = 100});
    ecs_override(world, bisonHerdPrefab, Group);
    ecs_set(world, bisonHerdPrefab, GrowthRate, {.stepsRequired = 360 * 24, .progress = 0});
    ecs_override(world, bisonHerdPrefab, GrowthRate);
    ecs_set(world, bisonHerdPrefab, ActorSize, {ACTOR_SIZE_LARGE});
}
