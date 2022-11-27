#ifndef BASALTIC_INTERACTION_H_INCLUDED
#define BASALTIC_INTERACTION_H_INCLUDED

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "basaltic_defs.h"
#include "basaltic_uiState.h"
#include "basaltic_logicInputState.h"

void bc_processInputEvent(bc_UiState *ui, bc_LogicInputState *logicInput, SDL_Event *e, bool useMouse, bool useKeyboard);
void bc_processInputState(bc_UiState *ui, bc_LogicInputState *logicInput, bool useMouse, bool useKeyboard);

#endif // BASALTIC_INTERACTION_H_INCLUDED
