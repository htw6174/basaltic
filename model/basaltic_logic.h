#ifndef BASALTIC_LOGIC_H_INCLUDED
#define BASALTIC_LOGIC_H_INCLUDED

#include "htw_core.h"
#include "basaltic_model.h"
#include "basaltic_worldState.h"
#include "basaltic_commandBuffer.h"

static const u32 BC_MAX_CHARACTERS = 10000;

// TODO: move this up to the model level
typedef enum bc_CommandType {
    BC_COMMAND_TYPE_STEP_ADVANCE,
    BC_COMMAND_TYPE_STEP_PLAY,
    BC_COMMAND_TYPE_STEP_PAUSE
} bc_CommandType;

// NOTE: because command data is copied into a different memory block exclusive to the logic thread and read asynchroniously, it should contain no pointers to view-side data. Pointers to model-side data should be fine. Now that most model data is contained in an ECS, use ECS entity IDs instead. Treat each command like a network data packet. If it needs to include variable length data, it should be appended to the end of the command with sizes+offsets in the struct to define it
typedef struct {
    bc_CommandType commandType;
} bc_WorldCommand;

bc_WorldState *bc_createWorldState(u32 chunkCountX, u32 chunkCountY, char* seedString);
int bc_initializeWorldState(bc_WorldState *world);
void bc_destroyWorldState(bc_WorldState *world);

int bc_doLogicTick(bc_ModelData *model, bc_CommandBuffer inputBuffer);
#endif // BASALTIC_LOGIC_H_INCLUDED
