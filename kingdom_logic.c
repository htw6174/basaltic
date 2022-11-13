#include <math.h>
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "kingdom_defs.h"
#include "kingdom_logic.h"
#include "kingdom_logicInputState.h"
#include "kingdom_worldState.h"

static void revealMap(kd_WorldState *world, kd_Character* character);

kd_WorldState *kd_createWorldState(u32 chunkCountX, u32 chunkCountY, u32 chunkWidth, u32 chunkHeight) {
    kd_WorldState *newWorld = malloc(sizeof(kd_WorldState));
    newWorld->chunkCountX = chunkCountX;
    newWorld->chunkCountY = chunkCountY;
    newWorld->chunkWidth = chunkWidth;
    newWorld->chunkHeight = chunkHeight;
    newWorld->worldWidth = chunkCountX * chunkWidth;
    newWorld->worldHeight = chunkCountY * chunkHeight;

    newWorld->characterPoolSize = CHARACTER_POOL_SIZE;
    return newWorld;
}

int kd_initializeWorldState(kd_WorldState *world) {
    // World generation
    u32 width = world->chunkWidth;
    u32 height = world->chunkHeight;
    u32 chunkCount = world->chunkCountX * world->chunkCountY;
    // TODO: single malloc for all world data, get offset of each array
    world->chunks = malloc(sizeof(kd_MapChunk) * chunkCount);
    for (int c = 0, y = 0; y < world->chunkCountY; y++) {
        for (int x = 0; x < world->chunkCountX; x++, c++) {
            // generate chunk data
            kd_MapChunk *chunk = &world->chunks[c];
            s32 cellPosX = x * width;
            s32 cellPosY = y * height;
            chunk->heightMap = htw_geo_createValueMap(width, height, 64);
            htw_geo_fillPerlin(chunk->heightMap, 6174, 6, cellPosX, cellPosY, 0.05);

            chunk->temperatureMap = htw_geo_createValueMap(width, height, world->chunkCountY * height);
            htw_geo_fillGradient(chunk->temperatureMap, cellPosY, cellPosY + height);

            chunk->rainfallMap = htw_geo_createValueMap(width, height, 255);
            htw_geo_fillPerlin(chunk->rainfallMap, 8, 2, cellPosX, cellPosY, 0.1);

            chunk->visibilityMap = htw_geo_createValueMap(width, height, UINT32_MAX);
            //htw_geo_fillChecker(chunk->visibilityMap, 0, 1, 4);
            htw_geo_fillUniform(chunk->visibilityMap, 0);
        }
    }

    // Populate world
    world->characters = malloc(sizeof(kd_Character) * world->characterPoolSize);
    for (int i = 0; i < world->characterPoolSize; i++) {
        // generate random character TODO
        kd_Character *newCharacter = &world->characters[i];
        *newCharacter = kd_createRandomCharacter();
        newCharacter->currentState.worldCoord = (htw_geo_GridCoord){htw_randRange(world->worldWidth), htw_randRange(world->worldHeight)};
    }

    return 0;
}

int kd_simulateWorld(kd_LogicInputState *input, kd_WorldState *world) {
    u64 ticks = input->ticks;

    if (input->isEditPending) {
        kd_MapEditAction action = input->currentEdit;
        htw_geo_GridCoord gridCoord = htw_geo_indexToGridCoord(action.cellIndex, world->chunkWidth);
        u32 chunkIndex = action.chunkIndex;
        s32 currentValue = htw_geo_getMapValue(world->chunks[chunkIndex].heightMap, gridCoord);
        htw_geo_setMapValue(world->chunks[chunkIndex].heightMap, gridCoord, currentValue + action.value);
        input->isEditPending = 0;
    }

    if (input->isMovePending) {
        kd_CharacterMoveAction action = input->currentMove;
        kd_Character *subject = &world->characters[action.characterId];
        htw_geo_GridCoord destCoord = kd_chunkAndCellToWorldCoordinates(world, action.chunkIndex, action.cellIndex);
        kd_moveCharacter(subject, destCoord);
        revealMap(world, subject);
        input->isMovePending = 0;
    }

    return 0;
}

// FIXME: why is the revealed area wrong when it crosses the horizontal world wrap boundary?
// Reveal an area of map around the target character's position according to their sight radius
static void revealMap(kd_WorldState *world, kd_Character* character) {
    // TODO: factor in terrain height, character size, and attributes e.g. isFlying
    // get character's current cell information
    htw_geo_GridCoord characterCoord = character->currentState.worldCoord;
    u32 charChunkIndex, charCellIndex;
    kd_gridCoordinatesToChunkAndCell(world, characterCoord, &charChunkIndex, &charCellIndex);
    u32 characterElevation = htw_geo_getMapValueByIndex(world->chunks[charChunkIndex].heightMap, charCellIndex);

    htw_geo_CubeCoord charCubeCoord = htw_geo_gridToCubeCoord(characterCoord);
    htw_geo_CubeCoord relativeCoord = {0, 0, 0};

    // get number of cells to check based on character's attributes
    u32 cellsInSightRange = htw_geo_getHexArea(4);
    u32 cellsInDetailRange = htw_geo_getHexArea(3);

    // TODO: because of the outward spiral cell iteration used here, it may be possible to keep track of cell attributes that affect visibility and apply them additively to more distant cells (would probably only make sense to do this in the same direction; would have to come up with a way to determine if a cell is 'behind' another), such as forests or high elevations blocking lower areas
    for (int c = 0; c < cellsInSightRange; c++) {
        htw_geo_CubeCoord worldCubeCoord = htw_geo_addCubeCoords(charCubeCoord, relativeCoord);
        htw_geo_GridCoord worldCoord = htw_geo_cubeToGridCoord(worldCubeCoord);
        u32 chunkIndex, cellIndex;
        kd_gridCoordinatesToChunkAndCell(world, worldCoord, &chunkIndex, &cellIndex); // FIXME: need to do bounds checking at some point in the chain here
        htw_ValueMap *visibilityMap = world->chunks[chunkIndex].visibilityMap;

        kd_TerrainVisibilityBitFlags cellVisibility = 0;
        // restrict sight by distance
        // NOTE: this only works because of the outward spiral iteration pattern. Should use cube coordinate distance instead for better reliability
        if (c < cellsInDetailRange) {
            cellVisibility = KD_TERRAIN_VISIBILITY_GEOMETRY | KD_TERRAIN_VISIBILITY_COLOR;
        } else {
            cellVisibility = KD_TERRAIN_VISIBILITY_GEOMETRY;
        }
        // restrict sight by relative elevation
        u32 elevation = htw_geo_getMapValueByIndex(world->chunks[chunkIndex].heightMap, cellIndex);
        if (elevation > characterElevation + 1) { // TODO: instead of constant +1, derive from character attributes
            cellVisibility = KD_TERRAIN_VISIBILITY_GEOMETRY;
        }

        u32 currentVisibility = htw_geo_getMapValueByIndex(visibilityMap, cellIndex);
        htw_geo_setMapValueByIndex(visibilityMap, cellIndex, currentVisibility | cellVisibility);

        htw_geo_getNextHexSpiralCoord(&relativeCoord);
    }
}
