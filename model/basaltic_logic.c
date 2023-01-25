#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "basaltic_defs.h"
#include "basaltic_logic.h"
#include "basaltic_model.h"
#include "basaltic_worldState.h"
#include "basaltic_commandBuffer.h"

typedef struct {
    bc_CommandBuffer processingBuffer;
    bool advanceSingleStep;
    bool autoStep;
} LogicState;

static void doWorldStep(bc_WorldState *world);

static void updateCharacters(bc_WorldState *world);

static void editMap(bc_WorldState *world, bc_TerrainEditCommand *terrainEdit);
static void moveCharacter(bc_WorldState *world, bc_CharacterMoveCommand *characterMove);
static void revealMap(bc_WorldState *world, bc_Character* character);

bc_WorldState *bc_createWorldState(u32 chunkCountX, u32 chunkCountY, char* seedString) {
    bc_WorldState *newWorld = malloc(sizeof(bc_WorldState));
    newWorld->seedString = calloc(BC_MAX_SEED_LENGTH, sizeof(char));
    strcpy(newWorld->seedString, seedString);
    newWorld->seed = xxh_hash(0, BC_MAX_SEED_LENGTH, (u8*)newWorld->seedString);
    newWorld->step = 0;

    newWorld->chunkCountX = chunkCountX;
    newWorld->chunkCountY = chunkCountY;
    newWorld->worldWidth = chunkCountX * bc_chunkSize;
    newWorld->worldHeight = chunkCountY * bc_chunkSize;

    newWorld->characterPoolSize = CHARACTER_POOL_SIZE;
    return newWorld;
}

int bc_initializeWorldState(bc_WorldState *world) {
    // World generation
    u32 width = bc_chunkSize;
    u32 height = bc_chunkSize;
    u32 chunkCount = world->chunkCountX * world->chunkCountY;
    // TODO: single malloc for all world data, get offset of each array
    world->chunks = malloc(sizeof(bc_MapChunk) * chunkCount);

    float worldCartesianWidth = htw_geo_hexToCartesianPositionX(world->worldWidth, 0);
    float worldCartesianHeight = htw_geo_hexToCartesianPositionY(world->worldHeight);

    for (int c = 0, y = 0; y < world->chunkCountY; y++) {
        for (int x = 0; x < world->chunkCountX; x++, c++) {
            // generate chunk data
            bc_MapChunk *chunk = &world->chunks[c];
            s32 cellPosX = x * width;
            s32 cellPosY = y * height;
            chunk->heightMap = htw_geo_createValueMap(width, height, 128);
            htw_geo_fillSimplex(chunk->heightMap, world->seed, 8, cellPosX, cellPosY, world->worldWidth, 4);

            chunk->temperatureMap = htw_geo_createValueMap(width, height, world->chunkCountY * height);
            //htw_geo_fillGradient(chunk->temperatureMap, cellPosY, cellPosY + height);
            //htw_geo_fillPerlin(chunk->temperatureMap, world->seed, 1, cellPosX, cellPosY, 0.05, worldCartesianWidth, worldCartesianHeight);
            htw_geo_fillSimplex(chunk->temperatureMap, world->seed + 1, 4, cellPosX, cellPosY, world->worldWidth, 4);

            chunk->rainfallMap = htw_geo_createValueMap(width, height, 255);
            //htw_geo_fillPerlin(chunk->rainfallMap, world->seed, 2, cellPosX, cellPosY, 0.1, worldCartesianWidth, worldCartesianHeight);
            htw_geo_fillSimplex(chunk->rainfallMap, world->seed + 2, 6, cellPosX, cellPosY, world->worldWidth, 4);
            //htw_geo_fillUniform(chunk->rainfallMap, 0);

            chunk->visibilityMap = htw_geo_createValueMap(width, height, UINT32_MAX);
            //htw_geo_fillChecker(chunk->visibilityMap, 0, 1, 4);
            htw_geo_fillUniform(chunk->visibilityMap, 0);
        }
    }

    // Populate world
    world->characters = malloc(sizeof(bc_Character) * world->characterPoolSize);
    for (int i = 0; i < world->characterPoolSize; i++) {
        // generate random character TODO
        bc_Character *newCharacter = &world->characters[i];
        *newCharacter = bc_createRandomCharacter();
        newCharacter->currentState.destinationCoord = newCharacter->currentState.worldCoord = (htw_geo_GridCoord){htw_randRange(world->worldWidth), htw_randRange(world->worldHeight)};
    }

    return 0;
}

void bc_destroyWorldState(bc_WorldState *world) {
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
                    case BC_COMMAND_TYPE_CHARACTER_MOVE:
                        moveCharacter(model->world, &currentCommand->characterMoveCommand);
                        break;
                    case BC_COMMAND_TYPE_TERRAIN_EDIT:
                        editMap(model->world, &currentCommand->terrainEditCommand);
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
    u32 chunkCount = world->chunkCountX * world->chunkCountY;
    u32 cellsPerChunk = bc_chunkSize * bc_chunkSize;
    for (int c = 0; c < chunkCount; c++) {
        htw_ValueMap *heightMap = world->chunks[c].heightMap;
        for (int i = 0; i < cellsPerChunk; i++) {
            s32 currentValue = htw_geo_getMapValueByIndex(heightMap, i);
            // With worldCoord lookup: ~60 tps
            // Without worldCoord lookup: ~140 tps
            htw_geo_GridCoord worldCoord = bc_chunkAndCellToWorldCoordinates(world, c, i);

            // running this block drops tps to *6*
            // Not unexpected considering all the conversions, lookups across chunk boundaries, and everything being unoptimized
            // Also this jumps from 262k valueMap lookups to 1.8m, more than 10m checked per second
            htw_geo_CubeCoord cubeHere = htw_geo_gridToCubeCoord(worldCoord);
            s32 neighborAverage = 0;
            for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
                htw_geo_CubeCoord cubeNeighbor = htw_geo_addCubeCoords(cubeHere, htw_geo_cubeDirections[d]);
                htw_geo_GridCoord gridNeighbor = htw_geo_cubeToGridCoord(cubeNeighbor);
                u32 chunkHere, cellHere;
                bc_gridCoordinatesToChunkAndCell(world, gridNeighbor, &chunkHere, &cellHere);
                neighborAverage += htw_geo_getMapValueByIndex(world->chunks[chunkHere].heightMap, cellHere);
            }
            neighborAverage /= HEX_DIRECTION_COUNT;
            s32 erosion = lerp_int(currentValue, neighborAverage, 0.2);
            htw_geo_setMapValueByIndex(heightMap, i, erosion);

            //s32 wave = sin(world->step / 20) * 2.0;
            //htw_geo_setMapValueByIndex(heightMap, i, currentValue + wave);
        }
    }
}

static void doWorldStep(bc_WorldState *world) {
    // TODO: all the rest
    //worldStepStressTest(world);

    updateCharacters(world);

    world->step++;
}

static void updateCharacters(bc_WorldState *world) {
    u32 characterCount = world->characterPoolSize;

    for (u32 i = 0; i < characterCount; i++) {
        bc_Character *subject = &world->characters[i];

        if (bc_doCharacterMove(subject)) {
            // character moved this turn
        } else {
            // rest
            // TODO: make rest it's own action which is defaulted to for player controlled characters when no other action is taken
            subject->currentState.currentStamina += 5;
        }
        if (subject->isControlledByPlayer) {
            revealMap(world, subject);
        }
    }
}

static void editMap(bc_WorldState *world, bc_TerrainEditCommand *terrainEdit) {
    // TODO: handle brush modes, sizes
    htw_ValueMap *heightMap = world->chunks[terrainEdit->chunkIndex].heightMap;
    s32 currentValue = htw_geo_getMapValueByIndex(heightMap, terrainEdit->cellIndex);
    htw_geo_setMapValueByIndex(heightMap, terrainEdit->cellIndex, currentValue + terrainEdit->value);
}

static void moveCharacter(bc_WorldState *world, bc_CharacterMoveCommand *characterMove) {
    // TODO: schedule move for next step instead of executing immediately
    htw_geo_GridCoord destCoord = bc_chunkAndCellToWorldCoordinates(world, characterMove->chunkIndex, characterMove->cellIndex);
    bc_setCharacterDestination(characterMove->subject, destCoord);
}

// FIXME: why is the revealed area wrong when it crosses the horizontal world wrap boundary?
// Reveal an area of map around the target character's position according to their sight radius
static void revealMap(bc_WorldState *world, bc_Character* character) {
    // TODO: factor in terrain height, character size, and attributes e.g. isFlying
    // get character's current cell information
    htw_geo_GridCoord characterCoord = character->currentState.worldCoord;
    u32 charChunkIndex, charCellIndex;
    bc_gridCoordinatesToChunkAndCell(world, characterCoord, &charChunkIndex, &charCellIndex);
    u32 characterElevation = htw_geo_getMapValueByIndex(world->chunks[charChunkIndex].heightMap, charCellIndex);

    htw_geo_CubeCoord charCubeCoord = htw_geo_gridToCubeCoord(characterCoord);
    htw_geo_CubeCoord relativeCoord = {0, 0, 0};

    // get number of cells to check based on character's attributes
    u32 sightRange = 6;
    u32 detailRange = sightRange - 1;
    u32 cellsInSightRange = htw_geo_getHexArea(sightRange);
    u32 cellsInDetailRange = htw_geo_getHexArea(detailRange);

    // TODO: because of the outward spiral cell iteration used here, it may be possible to keep track of cell attributes that affect visibility and apply them additively to more distant cells (would probably only make sense to do this in the same direction; would have to come up with a way to determine if a cell is 'behind' another), such as forests or high elevations blocking lower areas
    for (int c = 0; c < cellsInSightRange; c++) {
        htw_geo_CubeCoord worldCubeCoord = htw_geo_addCubeCoords(charCubeCoord, relativeCoord);
        htw_geo_GridCoord worldCoord = htw_geo_cubeToGridCoord(worldCubeCoord);
        u32 chunkIndex, cellIndex;
        bc_gridCoordinatesToChunkAndCell(world, worldCoord, &chunkIndex, &cellIndex);
        htw_ValueMap *visibilityMap = world->chunks[chunkIndex].visibilityMap;

        bc_TerrainVisibilityBitFlags cellVisibility = 0;
        // restrict sight by distance
        // NOTE: this only works because of the outward spiral iteration pattern. Should use cube coordinate distance instead for better reliability
        if (c < cellsInDetailRange) {
            cellVisibility = BC_TERRAIN_VISIBILITY_GEOMETRY | BC_TERRAIN_VISIBILITY_COLOR;
        } else {
            cellVisibility = BC_TERRAIN_VISIBILITY_GEOMETRY;
        }
        // restrict sight by relative elevation
        u32 elevation = htw_geo_getMapValueByIndex(world->chunks[chunkIndex].heightMap, cellIndex);
        if (elevation > characterElevation + 1) { // TODO: instead of constant +1, derive from character attributes
            cellVisibility = BC_TERRAIN_VISIBILITY_GEOMETRY;
        }

        u32 currentVisibility = htw_geo_getMapValueByIndex(visibilityMap, cellIndex);
        htw_geo_setMapValueByIndex(visibilityMap, cellIndex, currentVisibility | cellVisibility);

        htw_geo_getNextHexSpiralCoord(&relativeCoord);
    }
}
