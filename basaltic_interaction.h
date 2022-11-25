#ifndef KINGDOM_INTERACTION_H_INCLUDED
#define KINGDOM_INTERACTION_H_INCLUDED

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "basaltic_defs.h"
#include "basaltic_uiState.h"
#include "basaltic_logicInputState.h"

void bt_processInputEvent(bt_UiState *ui, bt_LogicInputState *logicInput, SDL_Event *e, bool useMouse, bool useKeyboard);
void bt_processInputState(bt_UiState *ui, bt_LogicInputState *logicInput, bool useMouse, bool useKeyboard);

#endif // KINGDOM_INTERACTION_H_INCLUDED
