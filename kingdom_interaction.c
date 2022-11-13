#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "htw_core.h"
#include "kingdom_interaction.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_SDL
#include "cimgui/cimgui.h"
#include "cimgui/cimgui_impl.h"

static void translateCamera(kd_UiState *ui, float xLocalMovement, float yLocalMovement);
static void snapCameraToCharacter(kd_UiState *ui);
static void editMap(kd_LogicInputState *logicInput, u32 chunkIndex, u32 cellIndex, s32 value);
static void moveCharacter(kd_LogicInputState *logicInput, u32 characterId, u32 chunkIndex, u32 cellIndex);

int kd_handleInputs(kd_UiState *ui, kd_LogicInputState *logicInput, KD_APPSTATE *volatile appState) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        ImGui_ImplSDL2_ProcessEvent(&e);
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
                    break;
                case SDLK_UP:
                    editMap(logicInput, ui->hoveredChunkIndex, ui->hoveredCellIndex, 1);
                    break;
                case SDLK_DOWN:
                    editMap(logicInput, ui->hoveredChunkIndex, ui->hoveredCellIndex, -1);
                    break;
            }
        }

        if (!igIsWindowHovered(ImGuiHoveredFlags_AnyWindow)) { // don't handle mouse events if mouse over an imgui window
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    //editMap(logicInput, ui->hoveredChunkIndex, ui->hoveredCellIndex, 1);
                    moveCharacter(logicInput, ui->activeCharacter, ui->hoveredChunkIndex, ui->hoveredCellIndex);
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    editMap(logicInput, ui->hoveredChunkIndex, ui->hoveredCellIndex, -1);
                }
            }
        }
    }

    // handle continuous button presses
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    // camera
    float xMovement = 0.0;
    float yMovement = 0.0;
    if (k[SDL_SCANCODE_A]) xMovement -= ui->cameraMovementSpeed * powf(ui->cameraDistance, 2.0) * 0.1;
    if (k[SDL_SCANCODE_D]) xMovement += ui->cameraMovementSpeed * powf(ui->cameraDistance, 2.0) * 0.1;
    if (k[SDL_SCANCODE_W]) yMovement += ui->cameraMovementSpeed * powf(ui->cameraDistance, 2.0) * 0.1;
    if (k[SDL_SCANCODE_S]) yMovement -= ui->cameraMovementSpeed * powf(ui->cameraDistance, 2.0) * 0.1;
    if (k[SDL_SCANCODE_Q]) ui->cameraYaw -= ui->cameraRotationSpeed;
    if (k[SDL_SCANCODE_E]) ui->cameraYaw += ui->cameraRotationSpeed;
    if (k[SDL_SCANCODE_R]) ui->cameraPitch += ui->cameraRotationSpeed;
    if (k[SDL_SCANCODE_F]) ui->cameraPitch -= ui->cameraRotationSpeed;
    if (k[SDL_SCANCODE_T]) ui->cameraElevation += ui->cameraMovementSpeed;
    if (k[SDL_SCANCODE_G]) ui->cameraElevation -= ui->cameraMovementSpeed;
    if (k[SDL_SCANCODE_Z]) ui->cameraDistance += ui->cameraMovementSpeed;
    if (k[SDL_SCANCODE_X]) ui->cameraDistance -= ui->cameraMovementSpeed;
    if (k[SDL_SCANCODE_C]) snapCameraToCharacter(ui);;

    translateCamera(ui, xMovement, yMovement);

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

static void translateCamera(kd_UiState *ui, float xLocalMovement, float yLocalMovement) {
    float yaw = ui->cameraYaw * DEG_TO_RAD;
    float sinYaw = sin(yaw);
    float cosYaw = cos(yaw);
    float xGlobalMovement = (xLocalMovement * cosYaw) + (yLocalMovement * -sinYaw);
    float yGlobalMovement = (yLocalMovement * cosYaw) + (xLocalMovement * sinYaw);

    ui->cameraX += xGlobalMovement;
    ui->cameraY += yGlobalMovement;
}

// TODO: oof this will require a few more imports, will at least need to know character grid position and hex grid position conversion
static void snapCameraToCharacter(kd_UiState *ui) {
    ui->cameraX = 0;
    ui->cameraY = 0;
    ui->cameraDistance = 5;
}

static void editMap(kd_LogicInputState *logicInput, u32 chunkIndex, u32 cellIndex, s32 value) {
            kd_MapEditAction newAction = {
                .editType = KD_MAP_EDIT_ADD,
                .chunkIndex = chunkIndex,
                .cellIndex = cellIndex,
                .value = value
            };
            logicInput->currentEdit = newAction;
            logicInput->isEditPending = 1;
}

static void moveCharacter(kd_LogicInputState *logicInput, u32 characterId, u32 chunkIndex, u32 cellIndex) {
    kd_CharacterMoveAction newMoveAction = {
        .characterId = characterId,
        .chunkIndex = chunkIndex,
        .cellIndex = cellIndex,
    };
    logicInput->currentMove = newMoveAction;
    logicInput->isMovePending = 1;
}
