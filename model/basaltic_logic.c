#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL_mutex.h>
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "basaltic_defs.h"
#include "basaltic_logic.h"
#include "basaltic_model.h"
#include "basaltic_worldState.h"
#include "basaltic_worldGen.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_components.h"
#include "components/basaltic_components_planes.h"
#include "basaltic_systems.h"
#include "flecs.h"
#include "khash.h"

typedef struct {
    bc_CommandBuffer processingBuffer;
    bool advanceSingleStep;
    bool autoStep;
} LogicState;

static void doWorldStep(bc_WorldState *world);

static void revealMap(ecs_world_t *world, htw_ChunkMap *cm, ecs_entity_t character);

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
    //ecs_set_stage_count(newWorld->ecsWorld, 2);
    //newWorld->readonlyWorld = ecs_get_stage(newWorld->ecsWorld, 1);
    ECS_IMPORT(newWorld->ecsWorld, BasalticComponents);
    ECS_IMPORT(newWorld->ecsWorld, BasalticSystems);

    htw_ChunkMap *cm = bc_createTerrain(chunkCountX, chunkCountY);
    newWorld->centralPlane = ecs_set(newWorld->ecsWorld, 0, Plane, {cm});

    newWorld->lock = SDL_CreateSemaphore(1);

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
    SDL_SemWait(world->lock);
    SDL_DestroySemaphore(world->lock);
    free(world->seedString);
    free(world);
}

int bc_doLogicTick(bc_ModelData *model, bc_CommandBuffer inputBuffer) {
    // Don't bother locking buffers if there are no pending commands
    if (!bc_commandBufferIsEmpty(inputBuffer)) {
        if (bc_transferCommandBuffer(model->processingBuffer, inputBuffer)) {
            u32 itemsInBuffer = bc_beginBufferProcessing(model->processingBuffer);
            for (int i = 0; i < itemsInBuffer; i++) {
                bc_WorldCommand *currentCommand = (bc_WorldCommand*)bc_getNextCommand(model->processingBuffer);
                if (currentCommand == NULL) {
                    break;
                }
                switch (currentCommand->commandType) {
                    case BC_COMMAND_TYPE_STEP_ADVANCE:
                        model->advanceSingleStep = true;
                        break;
                    case BC_COMMAND_TYPE_STEP_PLAY:
                        model->autoStep = true;
                        break;
                    case BC_COMMAND_TYPE_STEP_PAUSE:
                        model->autoStep = false;
                        break;
                    default:
                        fprintf(stderr, "ERROR: invalid command type %i", currentCommand->commandType);
                        break;
                }
            }
            bc_endBufferProcessing(model->processingBuffer);
        }
    }

    if (model->advanceSingleStep || model->autoStep) {
        doWorldStep(model->world);
        model->advanceSingleStep = false;
    }

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
        bc_CellData *cellData = cm->chunks[c].cellData;
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
                neighborAverage += ((bc_CellData*)htw_geo_getCell(cm, gridNeighbor))->height;
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
    // TODO: all the rest
    //worldStepStressTest(world);

    // Waiting here gives the view thread a chance to safely read & render world data
    SDL_SemWait(world->lock);
    ecs_progress(world->ecsWorld, 1.0);
    SDL_SemPost(world->lock);

    // ecs_world_t *stage = ecs_get_stage(world->ecsWorld, 0);
    // ecs_iter_t it = ecs_query_iter(stage, world->systems);
    // while (ecs_query_next(&it)) {
    //     for (int i = 0; i < it.count; i++) {
    //         ecs_run(stage, it.entities[i], 1.0, NULL);
    //     }
    // }
    // // TODO: only end readonly mode (flush the buffer and make ecs storage changes) if no other threads are currently reading from the world
    // ecs_readonly_end(world->ecsWorld);
    // ecs_readonly_begin(world->ecsWorld);

    world->step++;
}

/*
static void editMap(ecs_world_t *world, bc_TerrainEditCommand *terrainEdit) {
    // TODO: handle brush modes, sizes
    htw_ChunkMap *cm = ecs_get(world, terrainEdit->terrain, Plane)->chunkMap;
    bc_CellData *cell = cm->chunks[terrainEdit->chunkIndex].cellData;
    cell = &cell[terrainEdit->cellIndex];
    switch (terrainEdit->editType) {
        case BC_MAP_EDIT_MOUNTAIN:
            htw_geo_GridCoord coord = htw_geo_chunkAndCellToGridCoordinates(cm, terrainEdit->chunkIndex, terrainEdit->cellIndex);
            bc_wobbleLine(cm, coord, 128, 64);
            break;
        case BC_MAP_EDIT_ADD:
            cell->height += terrainEdit->value;
            break;
    }
}

static void moveCharacter(ecs_world_t *world, bc_CharacterMoveCommand *characterMove) {
    htw_ChunkMap *cm = ecs_get(world, characterMove->terrain, Plane)->chunkMap;
    htw_geo_GridCoord destCoord = htw_geo_chunkAndCellToGridCoordinates(cm, characterMove->chunkIndex, characterMove->cellIndex);

    // TEST: handle this with filters instead of specific entity
    ecs_filter_t *pcFilter = ecs_filter(world, {
        .terms = {
            {ecs_id(bc_GridDestination)},
            {ecs_pair(IsOn, EcsAny)},
            {PlayerControlled}
        }
    });
    ecs_iter_t it = ecs_filter_iter(world, pcFilter);
    while (ecs_filter_next(&it)) {
        bc_GridDestination *destinations = ecs_field(&it, bc_GridDestination, 1);
        for (int i = 0; i < it.count; i++) {
            destinations[i] = destCoord;
        }
    }

    //bc_setCharacterDestination(world, characterMove->subject, destCoord);
}*/
