#ifndef KINGDOM_UISTATE_H_INCLUDED
#define KINGDOM_UISTATE_H_INCLUDED

#include "htw_core.h"

typedef enum bt_CameraMode {
    KD_CAMERA_MODE_ORTHOGRAPHIC = 0,
    KD_CAMERA_MODE_PERSPECTIVE = 1
} bt_CameraMode;

typedef enum bt_WorldLayer { // TODO: make this into a general rendering layer mask?
    KD_WORLD_LAYER_SURFACE = 0,
    KD_WORLD_LAYER_CAVE = 1
} bt_WorldLayer;

typedef struct bt_Mouse {
    s32 x;
    s32 y;
    // position on last ui tick
    s32 lastX;
    s32 lasyY;
    // 0 when not held; if held, counts up by 1 per millisecond
    u32 leftHeld;
    u32 rightHeld;
} bt_Mouse;

typedef struct bt_UiState {
    // camera
    bt_CameraMode cameraMode;
    float cameraDistance;
    float cameraElevation;
    float cameraPitch;
    float cameraYaw;
    float cameraX;
    float cameraY;
    float cameraMovementSpeed;
    float cameraRotationSpeed;
    // world
    bt_WorldLayer activeLayer;
    bt_Mouse mouse;
    u32 hoveredChunkIndex;
    u32 hoveredCellIndex;
    u32 selectedCellIndex;
    // character control
    u32 activeCharacter;
    // menus
} bt_UiState;

bt_UiState bt_createUiState();

#endif // KINGDOM_UISTATE_H_INCLUDED
