#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "htw_core.h"
#include "htw_geomap.h"
#include "basaltic_interaction.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_logic.h"
#include "basaltic_characters.h"

static void translateCamera(bc_UiState *ui, float xLocalMovement, float yLocalMovement);
static void editMap(bc_CommandBuffer commandBuffer, u32 chunkIndex, u32 cellIndex, s32 value);
static void moveCharacter(bc_CommandBuffer commandBuffer, bc_Character *character, u32 chunkIndex, u32 cellIndex);
static void advanceStep(bc_CommandBuffer commandBuffer);

// TODO: add seperate input handling for each interfaceMode setting
void bc_processInputEvent(bc_UiState *ui, bc_CommandBuffer commandBuffer, SDL_Event *e, bool useMouse, bool useKeyboard) {
    if (useMouse && e->type == SDL_MOUSEBUTTONDOWN) {
        if (e->button.button == SDL_BUTTON_LEFT) {
            moveCharacter(commandBuffer, ui->activeCharacter, ui->hoveredChunkIndex, ui->hoveredCellIndex);
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            editMap(commandBuffer, ui->hoveredChunkIndex, ui->hoveredCellIndex, -1);
        }
    }
    if (useKeyboard && e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_p:
                ui->cameraMode = ui->cameraMode ^ 1; // invert
                break;
            case SDLK_c:
                bc_snapCameraToCharacter(ui, ui->activeCharacter);
                break;
            case SDLK_v:
                ui->activeLayer = ui->activeLayer ^ 1; // invert
                break;
            case SDLK_UP:
                editMap(commandBuffer, ui->hoveredChunkIndex, ui->hoveredCellIndex, 1);
                break;
            case SDLK_DOWN:
                editMap(commandBuffer, ui->hoveredChunkIndex, ui->hoveredCellIndex, -1);
                break;
            case SDLK_SPACE:
                advanceStep(commandBuffer);
                break;
        }
    }
}

void bc_processInputState(bc_UiState *ui, bc_CommandBuffer commandBuffer, bool useMouse, bool useKeyboard) {
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

void bc_snapCameraToCharacter(bc_UiState *ui, bc_Character *subject) {
    if (subject == NULL) return;
    htw_geo_GridCoord characterCoord = subject->currentState.worldCoord;
    htw_geo_getHexCellPositionSkewed(characterCoord, &ui->cameraX, &ui->cameraY);
    ui->cameraDistance = 5;
    // TODO: would like to set camera height also, but that requires inspecting world data as well. Maybe setup a general purpose height adjust in bc_window
}

static void translateCamera(bc_UiState *ui, float xLocalMovement, float yLocalMovement) {
    float yaw = ui->cameraYaw * DEG_TO_RAD;
    float sinYaw = sin(yaw);
    float cosYaw = cos(yaw);
    float xGlobalMovement = (xLocalMovement * cosYaw) + (yLocalMovement * -sinYaw);
    float yGlobalMovement = (yLocalMovement * cosYaw) + (xLocalMovement * sinYaw);

    ui->cameraX += xGlobalMovement;
    ui->cameraY += yGlobalMovement;

    // Wrap camera if outsize wrap limit:
    // Convert camera cartesian coordinates to hex space (expands y)
    // Check against camera wrap bounds
    // If out of bounds: wrap in hex space, convert back to cartesian space, and assign back to camera
    float cameraHexX = htw_geo_cartesianToHexPositionX(ui->cameraX, ui->cameraY);
    float cameraHexY = htw_geo_cartesianToHexPositionY(ui->cameraY);
    if (cameraHexX > ui->cameraWrapX) {
        cameraHexX -= ui->cameraWrapX * 2;
        ui->cameraX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
    } else if (cameraHexX < -ui->cameraWrapX) {
        cameraHexX += ui->cameraWrapX * 2;
        ui->cameraX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
    }

    if (cameraHexY > ui->cameraWrapY) {
        cameraHexY -= ui->cameraWrapY * 2;
        ui->cameraY = htw_geo_hexToCartesianPositionY(cameraHexY);
        // x position is dependent on y position when converting between hex and cartesian space, so also need to assign back to cameraX
        ui->cameraX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
    } else if (cameraHexY < -ui->cameraWrapY) {
        cameraHexY += ui->cameraWrapY * 2;
        ui->cameraY = htw_geo_hexToCartesianPositionY(cameraHexY);
        ui->cameraX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
    }

}

static void editMap(bc_CommandBuffer commandBuffer, u32 chunkIndex, u32 cellIndex, s32 value) {
    bc_TerrainEditCommand editCommand = {
        .editType = BC_MAP_EDIT_ADD,
        .brushSize = 1,
        .value = value,
        .chunkIndex = chunkIndex,
        .cellIndex = cellIndex
    };
    bc_WorldCommand inputCommand = {
        .commandType = BC_COMMAND_TYPE_TERRAIN_EDIT,
        .terrainEditCommand = editCommand,
    };
    bc_pushCommandToBuffer(commandBuffer, &inputCommand, sizeof(inputCommand));
}

static void moveCharacter(bc_CommandBuffer commandBuffer, bc_Character *character, u32 chunkIndex, u32 cellIndex) {
    if (character == NULL) return;
    bc_CharacterMoveCommand moveCommand = {
        .subject = character,
        .chunkIndex = chunkIndex,
        .cellIndex = cellIndex
    };
    bc_WorldCommand inputCommand = {
        .commandType = BC_COMMAND_TYPE_CHARACTER_MOVE,
        .characterMoveCommand = moveCommand
    };
    bc_pushCommandToBuffer(commandBuffer, &inputCommand, sizeof(inputCommand));
}

static void advanceStep(bc_CommandBuffer commandBuffer) {
    bc_WorldCommand stepCommand = {
        .commandType = BC_COMMAND_TYPE_STEP_ADVANCE,
    };
    bc_pushCommandToBuffer(commandBuffer, &stepCommand, sizeof(stepCommand));
}
