#ifndef KINGDOM_UISTATE_H_INCLUDED
#define KINGDOM_UISTATE_H_INCLUDED

typedef enum kd_CameraMode {
    KD_CAMERA_MODE_ORTHOGRAPHIC = 0,
    KD_CAMERA_MODE_PERSPECTIVE = 1
} kd_CameraMode;

typedef enum kd_WorldLayer { // TODO: make this into a general rendering layer mask?
    KD_WORLD_LAYER_SURFACE = 0,
    KD_WORLD_LAYER_CAVE = 1
} kd_WorldLayer;

typedef struct kd_Mouse {
    int x;
    int y;
    // position on last ui tick
    int lastX;
    int lasyY;
    // 0 when not held; if held, counts up by 1 per millisecond
    int leftHeld;
    int rightHeld;
} kd_Mouse;

typedef struct kd_UiState {
    kd_WorldLayer activeLayer;
    kd_CameraMode cameraMode;
    kd_Mouse mouse;
    float cameraDistance;
    float cameraPitch;
    float cameraYaw;
    float cameraX;
    float cameraY;
    float cameraMovementSpeed;
    float cameraRotationSpeed;
} kd_UiState;

kd_UiState kd_createUiState();

#endif // KINGDOM_UISTATE_H_INCLUDED
