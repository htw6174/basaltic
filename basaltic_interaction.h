#ifndef BASALTIC_INTERACTION_H_INCLUDED
#define BASALTIC_INTERACTION_H_INCLUDED

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "basaltic_commandQueue.h"
#include "basaltic_uiState.h"
#include "basaltic_characters.h"

void bc_processInputEvent(bc_UiState *ui, bc_CommandQueue commandQueue, SDL_Event *e, bool useMouse, bool useKeyboard);
void bc_processInputState(bc_UiState *ui, bc_CommandQueue commandQueue, bool useMouse, bool useKeyboard);
void bc_snapCameraToCharacter(bc_UiState *ui, bc_Character *subject);

#endif // BASALTIC_INTERACTION_H_INCLUDED
