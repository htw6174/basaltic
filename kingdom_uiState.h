#ifndef KINGDOM_UISTATE_H_INCLUDED
#define KINGDOM_UISTATE_H_INCLUDED

#include "htw_core.h"

typedef enum kd_CameraMode {
    KD_CAMERA_MODE_ORTHOGRAPHIC = 0,
    KD_CAMERA_MODE_PERSPECTIVE = 1
} kd_CameraMode;

typedef enum kd_WorldLayer { // TODO: make this into a general rendering layer mask?
    KD_WORLD_LAYER_SURFACE = 0,
    KD_WORLD_LAYER_CAVE = 1
} kd_WorldLayer;

typedef struct kd_Mouse {
    s32 x;
    s32 y;
    // position on last ui tick
    s32 lastX;
    s32 lasyY;
    // 0 when not held; if held, counts up by 1 per millisecond
    u32 leftHeld;
    u32 rightHeld;
} kd_Mouse;

typedef struct kd_UiState {
    // camera
    kd_CameraMode cameraMode;
    float cameraDistance;
    float cameraElevation;
    float cameraPitch;
    float cameraYaw;
    float cameraX;
    float cameraY;
    float cameraMovementSpeed;
    float cameraRotationSpeed;
    // world
    kd_WorldLayer activeLayer;
    kd_Mouse mouse;
    u32 hoveredChunkIndex;
    u32 hoveredCellIndex;
    u32 selectedCellIndex;
    // menus
} kd_UiState;

kd_UiState kd_createUiState();

#endif // KINGDOM_UISTATE_H_INCLUDED
