#include "kingdom_uiState.h"

kd_UiState kd_createUiState() {
    kd_UiState newUi = {
        // camera
        .cameraMode = KD_CAMERA_MODE_PERSPECTIVE,
        .cameraDistance = 10,
        .cameraElevation = 0,
        .cameraPitch = 45,
        .cameraYaw = 0,
        .cameraX = 0,
        .cameraY = 0,
        .cameraMovementSpeed = 0.15,
        .cameraRotationSpeed = 1.5,
        // world
        .activeLayer = KD_WORLD_LAYER_SURFACE,
        .mouse = {0},
        .hoveredCellIndex = 0,
        .selectedCellIndex = 0,
        // character control
        .activeCharacter = 0,
        // menus
    };
    return newUi;
}

