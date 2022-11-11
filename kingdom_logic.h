#ifndef KINGDOM_LOGIC_H_INCLUDED
#define KINGDOM_LOGIC_H_INCLUDED

#include "htw_core.h"
#include "kingdom_logicInputState.h"
#include "kingdom_worldState.h"

// TODO: constant or world creation parameter?
#define CHARACTER_POOL_SIZE 1024

kd_WorldState *kd_createWorldState(u32 chunkCountX, u32 chunkCountY, u32 chunkWidth, u32 chunkHeight);
int kd_initializeWorldState(kd_WorldState *world);
int kd_simulateWorld(kd_LogicInputState *input, kd_WorldState *world);

#endif // KINGDOM_LOGIC_H_INCLUDED
