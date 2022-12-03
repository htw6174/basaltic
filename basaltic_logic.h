#ifndef BASALTIC_LOGIC_H_INCLUDED
#define BASALTIC_LOGIC_H_INCLUDED

#include "htw_core.h"
#include "basaltic_worldState.h"
#include "basaltic_commandQueue.h"

// TODO: constant or world creation parameter?
#define CHARACTER_POOL_SIZE 1024

typedef struct {
    bc_WorldState *world;
    bc_CommandQueue inputQueue;
    u32 interval;
    volatile bc_ProcessState *threadState;
} bc_LogicThreadInput;

bc_WorldState *bc_createWorldState(u32 chunkCountX, u32 chunkCountY, char* seedString);
int bc_initializeWorldState(bc_WorldState *world);
void bc_destroyWorldState(bc_WorldState *world);

/**
 * @brief Should NOT be called directly, instead use with a call to SDL_CreateThread
 *
 * @param in must be a pointer to bc_LogicThreadInput
 * @return thread exit status
 */
int bc_runLogicThread(void *in);

#endif // BASALTIC_LOGIC_H_INCLUDED
