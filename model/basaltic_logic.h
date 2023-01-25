#ifndef BASALTIC_LOGIC_H_INCLUDED
#define BASALTIC_LOGIC_H_INCLUDED

#include "htw_core.h"
#include "basaltic_model.h"
#include "basaltic_worldState.h"
#include "basaltic_commandBuffer.h"

// TODO: constant or world creation parameter?
#define CHARACTER_POOL_SIZE 1024

typedef enum bc_CommandType {
    BC_COMMAND_TYPE_CHARACTER_MOVE,
    BC_COMMAND_TYPE_TERRAIN_EDIT,
    BC_COMMAND_TYPE_STEP_ADVANCE,
    BC_COMMAND_TYPE_STEP_PLAY,
    BC_COMMAND_TYPE_STEP_PAUSE
} bc_CommandType;

typedef enum bc_MapEditType {
    BC_MAP_EDIT_SET,
    BC_MAP_EDIT_ADD,
} bc_MapEditType;

typedef enum bc_TerrainVisibilityBitFlags {
    BC_TERRAIN_VISIBILITY_GEOMETRY =    0x00000001, // outline of cells and their topography (heightmap value)
    BC_TERRAIN_VISIBILITY_COLOR =       0x00000002, // base color from biome
    BC_TERRAIN_VISIBILITY_MEGALITHS =   0x00000004, // large ruins, cities, and castles. Usually visible whenever geometry is
    BC_TERRAIN_VISIBILITY_VEGETATION =  0x00000008, // presence or absence of trees and shrubs. Usually visible whenever color is
    BC_TERRAIN_VISIBILITY_STRUCTURES =  0x00000010, // roads, towns, and farmhouses in plain view
    BC_TERRAIN_VISIBILITY_MEGAFAUNA =   0x00000020, // huge size creatures or large herds, usually visible from a distance
    BC_TERRAIN_VISIBILITY_CHARACTERS =  0x00000040, // doesn't reveal specifics, just signs of life and activity (e.g. campfire smoke)
    BC_TERRAIN_VISIBILITY_SECRETS =     0x00000080, // hidden cave entrances, lairs, or isolated buildings
    BC_TERRAIN_VISIBILITY_ALL =         0xffffffff,
} bc_TerrainVisibilityBitFlags;

typedef struct {
    bc_MapEditType editType;
    u32 brushSize;
    s32 value;
    u32 chunkIndex;
    u32 cellIndex;
} bc_TerrainEditCommand;

typedef struct {
    bc_Character *subject;
    u32 chunkIndex;
    u32 cellIndex;
} bc_CharacterMoveCommand;


// NOTE: because command data is copied into a different memory block exclusive to the logic thread and read asynchroniously, it should contain **NO POINTERS**. Treat each command like a network data packet. If it needs to include variable length data, it should be appended to the end of the command with sizes+offsets in the struct to define it
typedef struct {
    bc_CommandType commandType;
    union {
        bc_TerrainEditCommand terrainEditCommand;
        bc_CharacterMoveCommand characterMoveCommand;
    };
} bc_WorldCommand;

bc_WorldState *bc_createWorldState(u32 chunkCountX, u32 chunkCountY, char* seedString);
int bc_initializeWorldState(bc_WorldState *world);
void bc_destroyWorldState(bc_WorldState *world);


int bc_doLogicTick(bc_ModelData *model, bc_CommandBuffer inputBuffer);
#endif // BASALTIC_LOGIC_H_INCLUDED
