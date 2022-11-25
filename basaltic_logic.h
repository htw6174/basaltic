#ifndef KINGDOM_LOGIC_H_INCLUDED
#define KINGDOM_LOGIC_H_INCLUDED

#include "htw_core.h"
#include "basaltic_logicInputState.h"
#include "basaltic_worldState.h"

// TODO: constant or world creation parameter?
#define CHARACTER_POOL_SIZE 1024

bt_WorldState *bt_createWorldState(u32 chunkCountX, u32 chunkCountY, u32 chunkWidth, u32 chunkHeight);
int bt_initializeWorldState(bt_WorldState *world);
int bt_simulateWorld(bt_LogicInputState *input, bt_WorldState *world);

#endif // KINGDOM_LOGIC_H_INCLUDED
