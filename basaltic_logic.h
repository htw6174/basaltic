#ifndef BASALTIC_LOGIC_H_INCLUDED
#define BASALTIC_LOGIC_H_INCLUDED

#include "htw_core.h"
#include "basaltic_logicInputState.h"
#include "basaltic_worldState.h"

// TODO: constant or world creation parameter?
#define CHARACTER_POOL_SIZE 1024

bc_WorldState *bc_createWorldState(u32 chunkCountX, u32 chunkCountY, u32 chunkWidth, u32 chunkHeight);
int bc_initializeWorldState(bc_WorldState *world);
int bc_simulateWorld(bc_LogicInputState *input, bc_WorldState *world);

#endif // BASALTIC_LOGIC_H_INCLUDED
