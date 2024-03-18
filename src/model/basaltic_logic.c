#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "basaltic_defs.h"
#include "basaltic_logic.h"
#include "basaltic_worldState.h"
#include "basaltic_worldGen.h"
#include "basaltic_components.h"
#include "components/basaltic_components_planes.h"
#include "basaltic_systems.h"
#include "flecs.h"
#include "khash.h"

#include "bc_flecs_utils.h"

static void doWorldStep(bc_WorldState *world);

bc_WorldState *bc_createWorldState(u32 chunkCountX, u32 chunkCountY, char* seedString) {
    bc_WorldState *newWorld = malloc(sizeof(bc_WorldState));
    newWorld->seedString = calloc(BC_MAX_SEED_LENGTH, sizeof(char));
    strcpy(newWorld->seedString, seedString);
    newWorld->seed = xxh_hash(0, BC_MAX_SEED_LENGTH, (u8*)newWorld->seedString);
    newWorld->step = 0;

#ifdef FLECS_SANITIZE
    printf("Initializing flecs in sanitizing mode. Expect a significant slowdown.\n");
#endif
    newWorld->ecsWorld = ecs_init();
    //ecs_set_threads(newWorld->ecsWorld, 4);
    //ecs_set_stage_count(newWorld->ecsWorld, 2);
    //newWorld->readonlyWorld = ecs_get_stage(newWorld->ecsWorld, 1);
    ECS_IMPORT(newWorld->ecsWorld, Bc);
    ECS_IMPORT(newWorld->ecsWorld, BcSystems);

    // TODO: script initialization method for ECS worlds to apply all scripts in a directory at startup
    ecs_plecs_from_file(newWorld->ecsWorld, "model/plecs/startup/startup_test.flecs");

    htw_ChunkMap *cm = bc_createTerrain(chunkCountX, chunkCountY);
    newWorld->centralPlane = ecs_set(newWorld->ecsWorld, 0, Plane, {cm});
    ecs_set_name(newWorld->ecsWorld, newWorld->centralPlane, "Overworld");

    // TODO: script initialization method for ECS worlds to apply all scripts in a directory at startup
    ecs_set_pair(newWorld->ecsWorld, 0, ResourceFile, FlecsScriptSource, {.path = "model/plecs/test.flecs"});

    return newWorld;
}

int bc_initializeWorldState(bc_WorldState *world) {

    // World generation
    bc_generateTerrain(ecs_get(world->ecsWorld, world->centralPlane, Plane)->chunkMap, world->seed);

    // Populate world
    //bc_createCharacters(world->ecsWorld, world->baseTerrain, 1);

    // ECS Queries (may be slightly faster to create these after creating entities)
    world->systems = ecs_query(world->ecsWorld, {
        .filter.terms = {
            {EcsSystem}
        }
    });
    world->planes = ecs_query(world->ecsWorld, {
        .filter.terms = {
            {ecs_id(Plane)}
        }
    });
    world->characters = ecs_query(world->ecsWorld, {
        .filter.terms = {
            {ecs_id(Position)},
            //{ecs_pair(IsOn, ecs_id(bc_TerrainMap))}
        }
    });

    // Progress single step to run EcsOnStart systems
    doWorldStep(world);

    return 0;
}

void bc_destroyWorldState(bc_WorldState *world) {
    // Ownership for queries is passed to the application, should clean them up manually
    // however, ecs_fini seems to take care of queries automatically
    //ecs_query_fini(world->terrains);
    //ecs_query_fini(world->characters);
    ecs_fini(world->ecsWorld);
    free(world->seedString);
    free(world);
}

int bc_doLogicTick(bc_WorldState *world) {
    doWorldStep(world);
    return 0;
}

static void worldStepStressTest(bc_WorldState *world) {

    // Stress TEST: do something with an area around every character, every frame
    // for 1024 characters with sight range 4, this is 38k updates
    // for (int i = 0; i < world->characterPoolSize; i++) {
    //     // runs at 160-190 tps
    //     revealMap(world, &world->characters[i]);
    // }

    // Stress TEST: do something with every tile, every frame
    // on an 8x8 chunk world, this is 262k updates
    htw_ChunkMap *cm = ecs_get(world->ecsWorld, world->centralPlane, Plane)->chunkMap;
    u32 chunkCount = cm->chunkCountX * cm->chunkCountY;
    u32 cellsPerChunk = bc_chunkSize * bc_chunkSize;
    for (int c = 0; c < chunkCount; c++) {
        CellData *cellData = cm->chunks[c].cellData;
        for (int i = 0; i < cellsPerChunk; i++) {
            s32 currentValue = cellData[i].height;
            // With worldCoord lookup: ~60 tps
            // Without worldCoord lookup: ~140 tps
            htw_geo_GridCoord worldCoord = htw_geo_chunkAndCellToGridCoordinates(cm, c, i);

            // running this block drops tps to *6*
            // Not unexpected considering all the conversions, lookups across chunk boundaries, and everything being unoptimized
            // Also this jumps from 262k valueMap lookups to 1.8m, more than 10m checked per second
            htw_geo_CubeCoord cubeHere = htw_geo_gridToCubeCoord(worldCoord);
            s32 neighborAverage = 0;
            for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
                htw_geo_CubeCoord cubeNeighbor = htw_geo_addCubeCoords(cubeHere, htw_geo_cubeDirections[d]);
                htw_geo_GridCoord gridNeighbor = htw_geo_cubeToGridCoord(cubeNeighbor);
                neighborAverage += ((CellData*)htw_geo_getCell(cm, gridNeighbor))->height;
            }
            neighborAverage /= HEX_DIRECTION_COUNT;
            s32 erosion = lerp_int(currentValue, neighborAverage, 0.2);
            cellData[i].height = erosion;

            //s32 wave = sin(world->step / 20) * 2.0;
            //htw_geo_setMapValueByIndex(heightMap, i, currentValue + wave);
        }
    }
}

static void doWorldStep(bc_WorldState *world) {
    bc_reloadFlecsScript(world->ecsWorld, 0);
    ecs_progress(world->ecsWorld, 1.0);
    world->step++;
    ecs_singleton_set(world->ecsWorld, Step, {world->step}); // FIXME kind of awkward to track this in 2 different places, but good reasons to keep both
}
