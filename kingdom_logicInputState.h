#ifndef KINGDOM_INPUTSTATE_H_INCLUDED
#define KINGDOM_INPUTSTATE_H_INCLUDED

#include <SDL_stdinc.h>

typedef struct {
    Uint32 ticks;
} kd_LogicInputState;

kd_LogicInputState *createLogicInputState();

#endif // KINGDOM_INPUTSTATE_H_INCLUDED
