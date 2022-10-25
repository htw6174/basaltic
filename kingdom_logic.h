#ifndef KINGDOM_LOGIC_H_INCLUDED
#define KINGDOM_LOGIC_H_INCLUDED

#include "htw_core.h"
#include "kingdom_logicInputState.h"
#include "kingdom_worldState.h"

kd_WorldState *kd_createWorldState(char *name);
int kd_initializeWorldState(kd_WorldState *world, unsigned int width, unsigned int height);
int kd_simulateWorld(kd_LogicInputState *input, kd_WorldState *world);

#endif // KINGDOM_LOGIC_H_INCLUDED
