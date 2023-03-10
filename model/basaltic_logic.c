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

    newWorld->surfaceMap = htw_geo_createChunkMap(bc_chunkSize, chunkCountX, chunkCountY, sizeof(bc_CellData));

    newWorld->characterPoolSize = CHARACTER_POOL_SIZE;
    return newWorld;
}

int bc_initializeWorldState(bc_WorldState *world) {
    // World generation
    u32 width = bc_chunkSize;
    u32 height = bc_chunkSize;
    u32 cellsPerChunk = width * height;
    u32 chunkCount = world->surfaceMap->chunkCountX * world->surfaceMap->chunkCountY;

    float worldCartesianWidth = htw_geo_hexToCartesianPositionX(world->surfaceMap->mapWidth, 0);
    float worldCartesianHeight = htw_geo_hexToCartesianPositionY(world->surfaceMap->mapHeight);

    for (int c = 0, y = 0; y < world->surfaceMap->chunkCountY; y++) {
        for (int x = 0; x < world->surfaceMap->chunkCountX; x++, c++) {
            // generate chunk data
            s32 cellPosX = x * width;
            s32 cellPosY = y * height;
            bc_CellData *cellData = world->surfaceMap->chunks[c].cellData;

            for (int i = 0; i < cellsPerChunk; i++) {
                bc_CellData *cell = &cellData[i];
                htw_geo_GridCoord cellCoord = htw_geo_chunkAndCellToGridCoordinates(world->surfaceMap, c, i);
                s32 grad = remap_int(cellCoord.y, 0, world->surfaceMap->mapHeight, 0, 255);
                s32 poleGrad1 = htw_geo_circularGradientByGridCoord(
                    world->surfaceMap, cellCoord, (htw_geo_GridCoord){world->surfaceMap->mapWidth / 2, world->surfaceMap->mapHeight / 2}, 255, 0, world->surfaceMap->mapWidth / 4.0);
                s32 poleGrad2 = htw_geo_circularGradientByGridCoord(
                    world->surfaceMap, cellCoord, (htw_geo_GridCoord){0, 0}, 255, 0, world->surfaceMap->mapWidth / 2.0);
                cell->height = 128;
                cell->temperature = poleGrad1;
                cell->nutrient = poleGrad2;
                cell->rainfall = 128;
                cell->visibility = 0;
            }
            // Max 128
            //htw_geo_fillSimplex(chunk->heightMap, world->seed, 8, cellPosX, cellPosY, world->worldWidth, 4);

            // Max of mapheight?
            //htw_geo_fillGradient(chunk->temperatureMap, cellPosY, cellPosY + height);
            //htw_geo_fillPerlin(chunk->temperatureMap, world->seed, 1, cellPosX, cellPosY, 0.05, worldCartesianWidth, worldCartesianHeight);
            //htw_geo_fillSimplex(chunk->temperatureMap, world->seed + 1, 4, cellPosX, cellPosY, world->worldWidth, 4);
            //htw_geo_fillCircularGradient(chunk->temperatureMap, (htw_geo_GridCoord){-cellPosX, -cellPosY}, 255, 0, world->worldWidth / 2);

            // Max of mapheight?
            //htw_geo_fillSimplex(chunk->nutrientMap, world->seed + 2, 4, cellPosX, cellPosY, world->worldWidth, 4);

            // Max 255
            //htw_geo_fillPerlin(chunk->rainfallMap, world->seed, 2, cellPosX, cellPosY, 0.1, worldCartesianWidth, worldCartesianHeight);
            //htw_geo_fillSimplex(chunk->rainfallMap, world->seed + 3, 6, cellPosX, cellPosY, world->worldWidth, 4);

            //htw_geo_fillChecker(chunk->visibilityMap, 0, 1, 4);
        }
    }

    // Populate world
    world->characters = malloc(sizeof(bc_Character) * world->characterPoolSize);
    for (int i = 0; i < world->characterPoolSize; i++) {
        // generate random character TODO
        bc_Character *newCharacter = &world->characters[i];
        *newCharacter = bc_createRandomCharacter();
        newCharacter->currentState.destinationCoord = newCharacter->currentState.worldCoord = (htw_geo_GridCoord){htw_randRange(world->surfaceMap->mapWidth), htw_randRange(world->surfaceMap->mapHeight)};
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
    u32 chunkCount = world->surfaceMap->chunkCountX * world->surfaceMap->chunkCountY;
    u32 cellsPerChunk = bc_chunkSize * bc_chunkSize;
    for (int c = 0; c < chunkCount; c++) {
        bc_CellData *cellData = world->surfaceMap->chunks[c].cellData;
        for (int i = 0; i < cellsPerChunk; i++) {
            s32 currentValue = cellData[i].height;
            // With worldCoord lookup: ~60 tps
            // Without worldCoord lookup: ~140 tps
            htw_geo_GridCoord worldCoord = htw_geo_chunkAndCellToGridCoordinates(world->surfaceMap, c, i);

            // running this block drops tps to *6*
            // Not unexpected considering all the conversions, lookups across chunk boundaries, and everything being unoptimized
            // Also this jumps from 262k valueMap lookups to 1.8m, more than 10m checked per second
            htw_geo_CubeCoord cubeHere = htw_geo_gridToCubeCoord(worldCoord);
            s32 neighborAverage = 0;
            for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
                htw_geo_CubeCoord cubeNeighbor = htw_geo_addCubeCoords(cubeHere, htw_geo_cubeDirections[d]);
                htw_geo_GridCoord gridNeighbor = htw_geo_cubeToGridCoord(cubeNeighbor);
                neighborAverage += ((bc_CellData*)htw_geo_getCell(world->surfaceMap, gridNeighbor))->height;
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
    bc_CellData *cell = world->surfaceMap->chunks[terrainEdit->chunkIndex].cellData;
    cell = &cell[terrainEdit->cellIndex];
    cell->height += terrainEdit->value;
}

static void moveCharacter(bc_WorldState *world, bc_CharacterMoveCommand *characterMove) {
    // TODO: schedule move for next step instead of executing immediately
    htw_geo_GridCoord destCoord = htw_geo_chunkAndCellToGridCoordinates(world->surfaceMap, characterMove->chunkIndex, characterMove->cellIndex);
    bc_setCharacterDestination(characterMove->subject, destCoord);
}

// FIXME: why is the revealed area wrong when it crosses the horizontal world wrap boundary?
// TODO: should take a specific chunkmap instead of entire world state
// Reveal an area of map around the target character's position according to their sight radius
static void revealMap(bc_WorldState *world, bc_Character* character) {
    // TODO: factor in terrain height, character size, and attributes e.g. isFlying
    // get character's current cell information
    htw_geo_GridCoord characterCoord = character->currentState.worldCoord;
    u32 charChunkIndex, charCellIndex;
    htw_geo_gridCoordinateToChunkAndCellIndex(world->surfaceMap, characterCoord, &charChunkIndex, &charCellIndex);
    s32 characterElevation = bc_getCellByIndex(world->surfaceMap, charChunkIndex, charCellIndex)->height;

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
        htw_geo_gridCoordinateToChunkAndCellIndex(world->surfaceMap, worldCoord, &chunkIndex, &cellIndex);
        bc_CellData *cell = bc_getCellByIndex(world->surfaceMap, chunkIndex, cellIndex);

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
