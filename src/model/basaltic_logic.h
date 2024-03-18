#ifndef BASALTIC_LOGIC_H_INCLUDED
#define BASALTIC_LOGIC_H_INCLUDED

#include "htw_core.h"
#include "basaltic_worldState.h"

static const u32 BC_MAX_CHARACTERS = 10000;

// TODO: move this up to the model level
typedef enum bc_CommandType {
    BC_COMMAND_TYPE_STEP_ADVANCE,
    BC_COMMAND_TYPE_STEP_PLAY,
    BC_COMMAND_TYPE_STEP_PAUSE
} bc_CommandType;

bc_WorldState *bc_createWorldState(u32 chunkCountX, u32 chunkCountY, char* seedString);
int bc_initializeWorldState(bc_WorldState *world);
void bc_destroyWorldState(bc_WorldState *world);

int bc_doLogicTick(bc_WorldState *world);
#endif // BASALTIC_LOGIC_H_INCLUDED
