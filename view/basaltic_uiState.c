#include "basaltic_uiState.h"

bc_UiState* bc_createUiState() {
    bc_UiState *ui = malloc(sizeof(bc_UiState));
    *ui = (bc_UiState){
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
        // ecs
        .world = NULL,
        // character control
        .focusedTerrain = 0,
        .activeCharacter = 0,
        // menus
    };
    return ui;
}

