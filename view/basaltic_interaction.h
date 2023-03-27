#ifndef BASALTIC_INTERACTION_H_INCLUDED
#define BASALTIC_INTERACTION_H_INCLUDED

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "htw_geomap.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_uiState.h"
#include "flecs.h"

void bc_processInputEvent(bc_UiState *ui, bc_CommandBuffer commandBuffer, SDL_Event *e, bool useMouse, bool useKeyboard);
void bc_processInputState(bc_UiState *ui, bc_CommandBuffer commandBuffer, bool useMouse, bool useKeyboard);

void bc_setModelEcsWorld(bc_UiState *ui, ecs_world_t *world);
void bc_setFocusedTerrain(bc_UiState *ui, ecs_entity_t terrain);
void bc_focusCameraOnCell(bc_UiState *ui, htw_geo_GridCoord cellCoord);
void bc_snapCameraToCharacter(bc_UiState *ui, ecs_entity_t e);

#endif // BASALTIC_INTERACTION_H_INCLUDED
