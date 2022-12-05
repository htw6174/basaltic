#include "basaltic_uiState.h"

bc_UiState bc_createUiState() {
    bc_UiState newUi = {
        .interfaceMode = BC_INTERFACE_MODE_SYSTEM_MENU,
        // camera
        .cameraMode = BC_CAMERA_MODE_PERSPECTIVE,
        .cameraDistance = 10,
        .cameraElevation = 0,
        .cameraPitch = 45,
        .cameraYaw = 0,
        .cameraX = 0,
        .cameraY = 0,
        .cameraMovementSpeed = 0.15,
        .cameraRotationSpeed = 1.5,
        // Must set these after initialization with bc_SetCameraWrapLimits
        .cameraWrapX = 0.0,
        .cameraWrapY = 0.0,
        // world
        .activeLayer = BC_WORLD_LAYER_SURFACE,
        .mouse = {0},
        .hoveredCellIndex = 0,
        .selectedCellIndex = 0,
        // character control
        .activeCharacter = NULL,
        // menus
    };
    return newUi;
}

void bc_SetCameraWrapLimits(bc_UiState *ui, u32 worldGridSizeX, u32 worldGridSizeY) {
    ui->cameraWrapX = worldGridSizeX / 2;
    ui->cameraWrapY = worldGridSizeY / 2;
}

