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

    // update logic input
    logicInput->ticks = SDL_GetTicks64();
    return 0;
}
