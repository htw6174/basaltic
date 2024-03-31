#include "basaltic_character_systems.h"
#include "basaltic_components.h"
#include "basaltic_phases.h"
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "flecs.h"
#include "bc_flecs_utils.h"
#include <float.h>
#include <math.h>

// TEST: random movement behavior, pick any adjacent tile to move to
void setWandererDestinations(ecs_iter_t *it);
// TEST: check neighboring cell features, move towards lowest elevation
void setDescenderDestinations(ecs_iter_t *it);
void revealMap(ecs_iter_t *it);

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

void egoBehaviorWander(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);

    for (int i = 0; i < it->count; i++) {
        destinations[i] = htw_geo_addGridCoords(positions[i], htw_geo_hexGridDirections[htw_randIndex(HEX_DIRECTION_COUNT)]);
        ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
    }
}

void setDescenderDestinations(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    const Plane *plane = ecs_field(it, Plane, 3);

    for (int i = 0; i < it->count; i++) {
        s32 lowestDirection = -1;
        s32 lowestElevation = ((CellData*)htw_geo_getCell(plane->chunkMap, positions[i]))->height;
        for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
            // Get cell elevation
            htw_geo_GridCoord evalCoord = htw_geo_addGridCoordsWrapped(plane->chunkMap, positions[i], htw_geo_hexGridDirections[d]);
            CellData *cell = (CellData*)htw_geo_getCell(plane->chunkMap, evalCoord);
            if (cell->height < lowestElevation) lowestDirection = d;
            lowestElevation = MIN(lowestElevation, cell->height);
        }
        if (lowestDirection == -1) {
            continue;
        } else {
            destinations[i] = htw_geo_addGridCoords(positions[i], htw_geo_hexGridDirections[lowestDirection]);
            ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
        }
    }
}

void revealMap(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Plane *plane = ecs_field(it, Plane, 2); // constant for each table
    htw_ChunkMap *cm = plane->chunkMap;
    MapVision *vis = ecs_field(it, MapVision, 3);

    for (int i = 0; i < it->count; i++) {
        // TODO: factor in terrain height, character size, and attributes e.g. Flying
        // get character's current cell information
        u32 charChunkIndex, charCellIndex;
        htw_geo_gridCoordinateToChunkAndCellIndex(cm, positions[i], &charChunkIndex, &charCellIndex);
        s32 characterElevation = bc_getCellByIndex(cm, charChunkIndex, charCellIndex)->height;

        htw_geo_CubeCoord charCubeCoord = htw_geo_gridToCubeCoord(positions[i]);
        htw_geo_CubeCoord relativeCoord = {0, 0, 0};

        u32 detailRange = vis[i].range;
        u32 sightRange = detailRange + 1;
        u32 cellsInDetailRange = htw_geo_getHexArea(detailRange);
        u32 cellsInSightRange = htw_geo_getHexArea(sightRange);

        // TODO: because of the outward spiral cell iteration used here, it may be possible to keep track of cell attributes that affect visibility and apply them additively to more distant cells (would probably only make sense to do this in the same direction; would have to come up with a way to determine if a cell is 'behind' another), such as forests or high elevations blocking lower areas
        for (int c = 0; c < cellsInSightRange; c++) {
            // Result coordinate is not confined to chunkmap, but converting to chunk and cell will also wrap input coords
            htw_geo_CubeCoord worldCubeCoord = htw_geo_addCubeCoords(charCubeCoord, relativeCoord);
            htw_geo_GridCoord worldCoord = htw_geo_cubeToGridCoord(worldCubeCoord);
            u32 chunkIndex, cellIndex;
            htw_geo_gridCoordinateToChunkAndCellIndex(cm, worldCoord, &chunkIndex, &cellIndex);
            CellData *cell = bc_getCellByIndex(cm, chunkIndex, cellIndex);

            u8 cellVisibility = 0;
            // restrict sight by distance
            // NOTE: this only works because of the outward spiral iteration pattern. Should use cube coordinate distance instead for better reliability
            if (c < cellsInDetailRange) {
                cellVisibility = 2; //BC_TERRAIN_VISIBILITY_GEOMETRY | BC_TERRAIN_VISIBILITY_COLOR;
            } else {
                cellVisibility = 1; //BC_TERRAIN_VISIBILITY_GEOMETRY;
            }
            // restrict sight by relative elevation
            s32 elevation = cell->height;
            if (elevation > characterElevation + 3) { // TODO: instead of constant, derive from character attributes
                cellVisibility = 1; //BC_TERRAIN_VISIBILITY_GEOMETRY;
            }

            cell->visibility = MAX(cell->visibility, cellVisibility);

            htw_geo_getNextHexSpiralCoord(&relativeCoord);
        }
    }
}

void characterCreated(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    ecs_entity_t plane = ecs_field_src(it, 2);

    for (int i = 0; i < it->count; i++) {
        plane_PlaceEntity(it->world, plane, it->entities[i], positions[i]);
    }
}

void characterMoved(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    ecs_entity_t plane = ecs_field_src(it, 2);
    for (int i = 0; i < it->count; i++) {
        plane_MoveEntity(it->world, plane, it->entities[i], positions[i]);
    }
}

void characterDestroyed(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    ecs_entity_t plane = ecs_field_src(it, 2);
    for (int i = 0; i < it->count; i++) {
        plane_RemoveEntity(it->world, plane, it->entities[i], positions[i]);
    }
}

void spawnActors(ecs_iter_t *it) {
    Spawner *spawners = ecs_field(it, Spawner, 1);
    Plane *plane = ecs_field(it, Plane, 2);
    ecs_entity_t planeEntity = ecs_field_src(it, 2);
    Step *step = ecs_field(it, Step, 3);

    ecs_world_t *world = it->world;
    htw_ChunkMap *cm = plane->chunkMap;

    ecs_entity_t oldScope = ecs_get_scope(world);
    //ecs_set_scope(world, plane); // TODO: best way to organize spawned entities for editor inspection?

    u32 maxX = cm->mapWidth;
    u32 maxY = cm->mapHeight;

    ecs_defer_suspend(world);
    for (int i = 0; i < it->count; i++) {
        Spawner sp = spawners[i];
        for (int e = 0; e < sp.count; e++) {
            // Must not defer operations to ensure that the same component pointer is accessed across multiple randomizers
            // Otherwise, the temporary storage returned from ecs_get_mut_id will overwrite earlier calls for the same component
            ecs_entity_t newCharacter = bc_instantiateRandomizer(world, sp.prefab);
            // TODO: do position randomization by adding randomizer to prefab?
            htw_geo_GridCoord coord = {
                .x = htw_randIndex(maxX),
                .y = htw_randIndex(maxY)
            };
            // THEORY: the observer doesn't trigger here, because the entity doesn't yet have both of these components before the function ends, and the merge doesn't trigger OnSet observers as expected
            ecs_add_pair(world, newCharacter, IsIn, planeEntity);
            // TODO: only set position to random map coord if the prefab has a tag like `RandomizePosition`
            ecs_set(world, newCharacter, Position, {coord.x, coord.y});
            ecs_set(world, newCharacter, CreationTime, {*step});
            // TODO: if instance has Elevation, set to current cell's height
            plane_PlaceEntity(world, planeEntity, newCharacter, coord);
        }
        if (sp.oneShot) {
            // Don't delete while iterating the same table
            ecs_defer_resume(world);
            ecs_delete(world, it->entities[i]);
            ecs_defer_suspend(world);
        }
    }
    ecs_defer_resume(world);

    ecs_set_scope(world, oldScope);
}

void egoBehaviorGrazer(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    htw_ChunkMap *cm = ecs_field(it, Plane, 3)->chunkMap;

    for (int i = 0; i < it->count; i++) {

        htw_geo_CubeCoord charCubeCoord = htw_geo_gridToCubeCoord(positions[i]);
        htw_geo_CubeCoord relativeCoord = {0, 0, 0};

        CellData *currentCell = htw_geo_getCell(cm, positions[i]);
        u32 availableHere = currentCell->understory;

        // if there is enough here, stay put and feed. Otherwise, find best nearby cell
        u32 tolerance = UINT32_MAX / 10; // If at least 10% grass, don't bother looking anywhere else
        if (availableHere >= tolerance) {
            ecs_add_pair(it->world, it->entities[i], Action, ActionFeed);
        } else {
            // get number of cells to check based on character's attributes
            u32 sightRange = 2;
            u32 cellsInSightRange = htw_geo_getHexArea(sightRange);
            // track best candidate
            float bestScore = FLT_MIN;
            CellData *cell = currentCell;
            htw_geo_CubeCoord bestDirection = {0, 0, 0};
            for (int c = 0; c < cellsInSightRange; c++) {
                // Result coordinate is not confined to chunkmap, but converting to chunk and cell will also wrap input coords
                htw_geo_CubeCoord worldCubeCoord = htw_geo_addCubeCoords(charCubeCoord, relativeCoord);
                htw_geo_GridCoord worldCoord = htw_geo_cubeToGridCoord(worldCubeCoord);
                u32 chunkIndex, cellIndex;
                htw_geo_gridCoordinateToChunkAndCellIndex(cm, worldCoord, &chunkIndex, &cellIndex);
                cell = bc_getCellByIndex(cm, chunkIndex, cellIndex);

                float score = 0;

                // exclude cells that can't be moved to
                s32 heightDiff = abs(currentCell->height - cell->height);
                bool canMoveToCell = heightDiff < 4 && cell->height >= 0;
                if (!canMoveToCell) {
                    htw_geo_getNextHexSpiralCoord(&relativeCoord);
                    continue;
                }

                const float tracksWeight = -5.0;
                const float grassWeight = 20.0;
                const float treeWeight = 1.0;
                const float waterWeight = 5.0;

                // Be sure to normalize these first, then work in percentages
                score += ((float)cell->tracks / UINT16_MAX) * tracksWeight;
                score += ((float)cell->understory / (float)UINT32_MAX) * grassWeight;
                score += -((float)cell->canopy / (float)UINT32_MAX) * treeWeight + (1.0 / 10.0); // positive from 0 to 10% of maximum, then drop off to negative
                score += ((float)cell->surfacewater / UINT16_MAX) * waterWeight;
                // TEST: add small random score to break ties and add variety? // TODO: need at least some way to force stacked groups appart, tend to overcollect in one place
                //score += htw_randInt(10);
                score += (float)xxh_hash2d(it->entities[i], worldCoord.x, worldCoord.y) / (float)UINT32_MAX;

                if (score > bestScore) {
                    bestScore = score;
                    bestDirection = relativeCoord;
                }

                htw_geo_getNextHexSpiralCoord(&relativeCoord);
            }

            // Current cell is also scored. If best cell is current cell, then stay and feed
            // NOTE: possible that there is no grass here at all, in which case this action shouldn't be choosen. However, tracks accumulation should prevent this from happening for more than a couple hours in a row
            if (htw_geo_isEqualCubeCoords(bestDirection, (htw_geo_CubeCoord){0, 0, 0})) {
                ecs_add_pair(it->world, it->entities[i], Action, ActionFeed);
            } else {
                // Apply best direction found
                destinations[i] = htw_geo_addGridCoords(positions[i], htw_geo_cubeToGridCoord(bestDirection));
                ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
            }
        }
    }
}

// FIXME: should use same scoring system grazers use
void egoBehaviorPredator(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    htw_ChunkMap *cm = ecs_field(it, Plane, 3)->chunkMap;
    ecs_entity_t planeEntity = ecs_field_src(it, 3);

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
            ecs_entity_t root = plane_GetRootEntity(it->world, planeEntity, worldCoord);
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
            destinations[i] = htw_geo_addGridCoords(positions[i], htw_geo_hexGridDirections[htw_randIndex(HEX_DIRECTION_COUNT)]);
        }
        ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
    }
}

void executeMove(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    const Plane *plane = ecs_field(it, Plane, 3);
    ecs_entity_t planeEntity = ecs_field_src(it, 3);

    if (ecs_field_is_set(it, 4)) {
        Group *groups = ecs_field(it, Group, 4);

        for (int i = 0; i < it->count; i++) {
            u32 movementDistance = htw_geo_hexGridDistance(positions[i], destinations[i]);
            // TODO: move along each cell in path, leaving tracks on each
            CellData *cell = htw_geo_getCell(plane->chunkMap, positions[i]);
            s64 tracks = cell->tracks;
            tracks += groups[i].count;
            cell->tracks = MIN(tracks, UINT16_MAX);

            // TODO: move towards destination by maximum single turn move distance
            plane_MoveEntity(it->world, planeEntity, it->entities[i], destinations[i]);
            // TODO: if entity has stamina, deduct stamina (or add if no move taken. Should maybe be in a seperate system)
            positions[i] = htw_geo_wrapGridCoordOnChunkMap(plane->chunkMap, destinations[i]);
        }
    } else {
        for (int i = 0; i < it->count; i++) {
            u32 movementDistance = htw_geo_hexGridDistance(positions[i], destinations[i]);
            // TODO: move towards destination by maximum single turn move distance
            plane_MoveEntity(it->world, planeEntity, it->entities[i], destinations[i]);
            // TODO: if entity has stamina, deduct stamina (or add if no move taken. Should maybe be in a seperate system)
            positions[i] = htw_geo_wrapGridCoordOnChunkMap(plane->chunkMap, destinations[i]);
        }
    }
}

void executeFeed(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    htw_ChunkMap *cm = ecs_field(it, Plane, 2)->chunkMap;
    ecs_entity_t diet = ecs_pair_second(it->world, ecs_field_id(it, 3));
    Condition *conditions = ecs_field(it, Condition, 4);
    if (ecs_field_is_set(it, 5)) {
        // multiply consumed amount by group size
        Group *groups = ecs_field(it, Group, 5);
        for (int i = 0; i < it->count; i++) {
            CellData *cell = htw_geo_getCell(cm, positions[i]);
            s64 understory = cell->understory;
            s64 tracks = cell->tracks;
            s64 required = groups[i].count * 4096 * 5; // TODO: multiply by size?; TODO: this number is a hacky way to represent expected consumption with 32-bit veg range
            s64 consumed = MIN(understory, required);
            understory -= consumed;
            tracks += groups[i].count; // * size^2?
            cell->understory = MAX(0, understory);
            cell->tracks = MIN(tracks, UINT16_MAX);
            // Restore up to half of max each meal TODO best rounding method?
            s32 restored = roundf( ((float)conditions[i].maxStamina / 2) * ((float)consumed / required) );
            conditions[i].stamina = MIN(conditions[i].stamina + restored, conditions[i].maxStamina);
        }
    } else {
        for (int i = 0; i < it->count; i++) {
            CellData *cell = htw_geo_getCell(cm, positions[i]);
            s32 consumed = MIN(cell->understory, 1);
            cell->understory -= consumed;
            s32 restored = conditions[i].maxStamina / 2; // Restore half of max each meal
            conditions[i].stamina = MIN(conditions[i].stamina + restored, conditions[i].maxStamina);
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
            conditions[i].health = MIN(conditions[i].health + 1, conditions[i].maxHealth);
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
    ecs_entity_t planeEntity = ecs_field_src(it, 2);
    Group *groups = ecs_field(it, Group, 3);
    ecs_entity_t prefab = ecs_pair_second(it->world, ecs_field_id(it, 4));

    for (int i = 0; i < it->count; i++) {

        ecs_entity_t root = plane_GetRootEntity(it->world, planeEntity, positions[i]);
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
        growthRates[i].progress += groups[i].count * (24 / 2); // Once per hour for every 2 in the group
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
        conditions[i].stamina = MAX(-maxStam, newStam);
    }
}

void BcSystemsCharactersImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystemsCharacters);

    // ECS_IMPORT(world, Bc); // NOTE: this import does nothing, because `Bc` is a 'parent' path of `BcSystemsCharacters`
    ECS_IMPORT(world, BcCommon);
    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);
    ECS_IMPORT(world, BcActors);
    ECS_IMPORT(world, BcWildlife);

    ECS_SYSTEM(world, egoBehaviorWander, Planning,
        [in] Position,
        [out] Destination,
        [in] Plane(up(bc.planes.IsIn)),
        [none] (bc.actors.Ego, bc.actors.Ego.EgoWanderer)
    );

    ECS_SYSTEM(world, revealMap, Resolution,
        [in] Position,
        [in] Plane(up(bc.planes.IsIn)),
        [in] MapVision
    );

    // TODO: try out observers only for position changes
    // NOTE: observers can propogate events along traversable relationships, meaning that when Plane is set, this event triggers for all entities on that plane
    // ECS_OBSERVER(world, characterCreated, EcsOnAdd,
    //     [in] Position,
    //     [none] Plane(up(bc.planes.IsIn))
    // );
    // ECS_OBSERVER(world, characterMoved, EcsOnSet,
    //     [in] Position,
    //     Plane(up(bc.planes.IsIn))
    // );
    // ECS_OBSERVER(world, characterDestroyed, EcsOnDelete,
    //     Position,
    //     Plane(up(bc.planes.IsIn))
    // );

    ECS_SYSTEM(world, spawnActors, EcsPreUpdate,
        [in] Spawner,
        [in] Plane(up(bc.planes.IsIn)),
        [in] Step($)
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
               [in] Plane(up(bc.planes.IsIn)),
               [none] (bc.actors.Ego, bc.actors.Ego.EgoGrazer),
    );
    ecs_system(world, {
        .entity = egoBehaviorGrazer,
        .multi_threaded = true
    });

    ECS_SYSTEM(world, egoBehaviorPredator, Planning,
               [in] Position,
               [out] Destination,
               [in] Plane(up(bc.planes.IsIn)),
               [none] (bc.actors.Ego, bc.actors.Ego.EgoPredator),
    );
    ecs_system(world, {
        .entity = egoBehaviorPredator,
        .multi_threaded = true
    });

    ECS_SYSTEM(world, executeMove, Execution,
               [out] Position,
               [in] Destination,
               [in] Plane(up(bc.planes.IsIn)),
               [in] ?Group,
               [none] (bc.actors.Action, bc.actors.Action.ActionMove)
    );
    // movement actions must be synchronious TODO: revisit this; could be enough to have a cleanup step to enforce the 1 root per cell rule
    ecs_system(world, {
        .entity = executeMove,
        .no_readonly = true
    });

    ECS_SYSTEM(world, executeFeed, Execution,
               [in] Position,
               [in] Plane(up(bc.planes.IsIn)),
               [in] (bc.wildlife.Diet, _),
               [in] Condition,
               [in] ?Group,
               [none] (bc.actors.Action, bc.actors.Action.ActionFeed)
    );

    ECS_SYSTEM(world, resolveHealth, Resolution,
               [inout] Condition,
               [inout] Group
    );

    // TODO: might want to remove this system and instead make group splitting and merging the outcome of an event, which has a chance of appearing when 2 groups cross paths or a group is large enough
    ECS_SYSTEM(world, mergeGroups, Cleanup,
               [in] Position,
               [in] Plane(up(bc.planes.IsIn)),
               [inout] Group,
               [in] (IsA, _), // TODO: consider a better way to determine merge compatability. Can't use query variables for cached queries
               [none] bc.planes.CellRoot(parent)
    );
    ecs_system(world, {
        .entity = mergeGroups,
        .no_readonly = true
    });

    ECS_SYSTEM(world, tickGrowth, AdvanceStep,
        [inout] GrowthRate,
        [inout] Group
    );
    ecs_set_tick_source(world, tickGrowth, TickDay);

    ECS_SYSTEM(world, tickStamina, AdvanceStep,
        [inout] Condition
    );
}
