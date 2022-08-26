#include "kingdom_uiState.h"

kd_UiState kd_createUiState() {
    kd_UiState newUi = {
        .cameraMode = KD_CAMERA_MODE_PERSPECTIVE,
        .cameraDistance = 10,
        .cameraPitch = 45,
        .cameraYaw = 0,
        .cameraX = 0,
        .cameraY = 0,
        .cameraMovementSpeed = 0.15,
        .cameraRotationSpeed = 1.5
    };
    return newUi;
}

