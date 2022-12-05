#ifndef BASALTIC_UISTATE_H_INCLUDED
#define BASALTIC_UISTATE_H_INCLUDED

#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_characters.h"

typedef enum bc_CameraMode {
    BC_CAMERA_MODE_ORTHOGRAPHIC = 0,
    BC_CAMERA_MODE_PERSPECTIVE = 1
} bc_CameraMode;

typedef enum bc_WorldLayer { // TODO: make this into a general rendering layer mask?
    BC_WORLD_LAYER_SURFACE = 0,
    BC_WORLD_LAYER_CAVE = 1
} bc_WorldLayer;

typedef struct bc_Mouse {
    s32 x;
    s32 y;
    // position on last ui tick
    s32 lastX;
    s32 lasyY;
    // 0 when not held; if held, counts up by 1 per millisecond
    u32 leftHeld;
    u32 rightHeld;
} bc_Mouse;

typedef struct bc_UiState {
    bc_InterfaceMode interfaceMode;
    // camera
    bc_CameraMode cameraMode;
    float cameraDistance;
    float cameraElevation;
    float cameraPitch;
    float cameraYaw;
    float cameraX;
    float cameraY;
    // camera limits
    float cameraMovementSpeed;
    float cameraRotationSpeed;
    // max camera distance from origin, in hex coordinate space
    float cameraWrapX;
    float cameraWrapY;
    // world
    bc_WorldLayer activeLayer;
    bc_Mouse mouse;
    u32 hoveredChunkIndex;
    u32 hoveredCellIndex;
    u32 selectedCellIndex;
    // character control
    bc_Character *activeCharacter;
    // menus
} bc_UiState;

bc_UiState bc_createUiState();
void bc_SetCameraWrapLimits(bc_UiState *ui, u32 worldGridSizeX, u32 worldGridSizeY);

#endif // BASALTIC_UISTATE_H_INCLUDED
