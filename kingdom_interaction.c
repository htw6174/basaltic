#include <stdio.h>
#include <SDL2/SDL.h>
#include "kingdom_interaction.h"

int kd_handleInputs(kd_UiState *ui, kd_LogicInputState *logicInput, KD_APPSTATE *volatile appState) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            *appState = KD_APPSTATE_STOPPED;
            return 0;
        }
        // handle single press events
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_p:
                    ui->cameraMode = ui->cameraMode ^ 1; // invert
                    break;
                case SDLK_c:
                    ui->activeLayer = ui->activeLayer ^ 1; // invert
            }
        }
        else if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT) {
                //printf("clicked cell %u\n", ui->hoveredCellIndex);
                kd_MapEditAction newAction = {
                    .editType = KD_MAP_EDIT_ADD,
                    .cellIndex = ui->hoveredCellIndex,
                    .value = 1
                };
                logicInput->currentEdit = newAction;
                logicInput->isEditPending = 1;
            }
        }
    }

    // handle continuous button presses
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    if (k[SDL_SCANCODE_A]) ui->cameraX -= ui->cameraMovementSpeed;
    if (k[SDL_SCANCODE_D]) ui->cameraX += ui->cameraMovementSpeed;
    if (k[SDL_SCANCODE_W]) ui->cameraY += ui->cameraMovementSpeed;
    if (k[SDL_SCANCODE_S]) ui->cameraY -= ui->cameraMovementSpeed;
    if (k[SDL_SCANCODE_Q]) ui->cameraYaw -= ui->cameraRotationSpeed;
    if (k[SDL_SCANCODE_E]) ui->cameraYaw += ui->cameraRotationSpeed;
    if (k[SDL_SCANCODE_R]) ui->cameraPitch += ui->cameraRotationSpeed;
    if (k[SDL_SCANCODE_F]) ui->cameraPitch -= ui->cameraRotationSpeed;
    if (k[SDL_SCANCODE_Z]) ui->cameraDistance += ui->cameraMovementSpeed;
    if (k[SDL_SCANCODE_X]) ui->cameraDistance -= ui->cameraMovementSpeed;

    // get mouse state
    Uint32 mouseStateMask = SDL_GetMouseState(&ui->mouse.x, &ui->mouse.y);
    //ui->mouse.left =
    if (mouseStateMask & SDL_BUTTON_LMASK) ui->mouse.leftHeld += 1; // TODO: add frame time instead of constant
    else ui->mouse.leftHeld = 0;

    // handle mouse events


    // update logic input
    logicInput->ticks = SDL_GetTicks64();
    return 0;
}
