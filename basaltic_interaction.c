#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "htw_core.h"
#include "htw_geomap.h"
#include "basaltic_interaction.h"
#include "basaltic_logicInputState.h"
#include "basaltic_characters.h"

static void translateCamera(bc_UiState *ui, float xLocalMovement, float yLocalMovement);
static void snapCameraToCharacter(bc_UiState *ui);
static void editMap(bc_LogicInputState *logicInput, u32 chunkIndex, u32 cellIndex, s32 value);
static void moveCharacter(bc_LogicInputState *logicInput, u32 characterId, u32 chunkIndex, u32 cellIndex);

void bc_processInputEvent(bc_UiState *ui, bc_LogicInputState *logicInput, SDL_Event *e, bool useMouse, bool useKeyboard) {
    if (useMouse && e->type == SDL_MOUSEBUTTONDOWN) {
        if (e->button.button == SDL_BUTTON_LEFT) {
            //editMap(logicInput, ui->hoveredChunkIndex, ui->hoveredCellIndex, 1);
            moveCharacter(logicInput, ui->activeCharacter, ui->hoveredChunkIndex, ui->hoveredCellIndex);
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            editMap(logicInput, ui->hoveredChunkIndex, ui->hoveredCellIndex, -1);
        }
    }
    if (useKeyboard && e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_p:
                ui->cameraMode = ui->cameraMode ^ 1; // invert
                break;
            case SDLK_c:
                snapCameraToCharacter(ui);
                break;
            case SDLK_v:
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
}

void bc_processInputState(bc_UiState *ui, bc_LogicInputState *logicInput, bool useMouse, bool useKeyboard) {
    if (useMouse) {
        // get mouse state
        Uint32 mouseStateMask = SDL_GetMouseState(&ui->mouse.x, &ui->mouse.y);
        if (mouseStateMask & SDL_BUTTON_LMASK) ui->mouse.leftHeld += 1; // TODO: add frame time instead of constant
        else ui->mouse.leftHeld = 0;
    }

    // camera
    float xMovement = 0.0;
    float yMovement = 0.0;
    if (useKeyboard) {
        // handle continuous button presses
        const Uint8 *k = SDL_GetKeyboardState(NULL);
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
    }

    translateCamera(ui, xMovement, yMovement);
}

static void translateCamera(bc_UiState *ui, float xLocalMovement, float yLocalMovement) {
    float yaw = ui->cameraYaw * DEG_TO_RAD;
    float sinYaw = sin(yaw);
    float cosYaw = cos(yaw);
    float xGlobalMovement = (xLocalMovement * cosYaw) + (yLocalMovement * -sinYaw);
    float yGlobalMovement = (yLocalMovement * cosYaw) + (xLocalMovement * sinYaw);

    ui->cameraX += xGlobalMovement;
    ui->cameraY += yGlobalMovement;
}

// TODO: oof this will require a few more imports, will at least need to know character grid position and hex grid position conversion
static void snapCameraToCharacter(bc_UiState *ui) {
    ui->cameraX = 0;
    ui->cameraY = 0;
    ui->cameraDistance = 5;
}

static void editMap(bc_LogicInputState *logicInput, u32 chunkIndex, u32 cellIndex, s32 value) {
            bc_MapEditAction newAction = {
                .editType = BC_MAP_EDIT_ADD,
                .chunkIndex = chunkIndex,
                .cellIndex = cellIndex,
                .value = value
            };
            logicInput->currentEdit = newAction;
            logicInput->isEditPending = 1;
}

static void moveCharacter(bc_LogicInputState *logicInput, u32 characterId, u32 chunkIndex, u32 cellIndex) {
    bc_CharacterMoveAction newMoveAction = {
        .characterId = characterId,
        .chunkIndex = chunkIndex,
        .cellIndex = cellIndex,
    };
    logicInput->currentMove = newMoveAction;
    logicInput->isMovePending = 1;
}
