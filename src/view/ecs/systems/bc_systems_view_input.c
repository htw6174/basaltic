#include "bc_systems_view_input.h"
#include "basaltic_components_view.h"
#include "htw_core.h"
#include "bc_flecs_utils.h"
#include <math.h>
#include <SDL2/SDL.h>

void LockMouse(ecs_iter_t *it) {
    SDL_SetRelativeMouseMode(true);
}

void UnlockMouse(ecs_iter_t *it) {
    SDL_SetRelativeMouseMode(false);
}

void CameraPanMouse(ecs_iter_t *it) {
    Camera *cam = ecs_field(it, Camera, 1);
    Pointer *pointer = ecs_field(it, Pointer, 2);

    float dT = it->delta_time;
    float speed = powf(cam->distance, 2.0) * dT * 0.1;
    if (ecs_field_is_set(it, 3)) {
        CameraSpeed camSpeed = *ecs_field(it, CameraSpeed, 3);
        speed *= camSpeed.movement;
    }
    float dX = (pointer->x - pointer->lastX) * speed * -1.0; // flip sign to match expected movement direction
    float dY = (pointer->y - pointer->lastY) * speed;
    // No need to modify z here; TODO: system to drift camera origin.z toward terrain height at origin.xy
    vec3 camDelta = (vec3){{dX, dY, 0.0}};

    float yaw = cam->yaw * DEG_TO_RAD;
    float sinYaw = sin(yaw);
    float cosYaw = cos(yaw);
    float xGlobalMovement = (camDelta.x * cosYaw) + (camDelta.y * -sinYaw);
    float yGlobalMovement = (camDelta.y * cosYaw) + (camDelta.x * sinYaw);

    float unwrappedX = cam->origin.x + xGlobalMovement;
    float unwrappedY = cam->origin.y + yGlobalMovement;
    float wrappedX = unwrappedX;
    float wrappedY = unwrappedY;

    if (ecs_field_is_set(it, 4)) {
        CameraWrap wrap = *ecs_field(it, CameraWrap, 4);
        // Wrap camera if outsize wrap limit:
        // Convert camera cartesian coordinates to hex space (expands y)
        // Check against camera wrap bounds
        // If out of bounds: wrap in hex space, convert back to cartesian space, and assign back to camera
        float cameraHexX = htw_geo_cartesianToHexPositionX(unwrappedX, unwrappedY);
        float cameraHexY = htw_geo_cartesianToHexPositionY(unwrappedY);
        if (cameraHexX > wrap.x) {
            cameraHexX -= wrap.x * 2;
            wrappedX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
        } else if (cameraHexX < -wrap.x) {
            cameraHexX += wrap.x * 2;
            wrappedX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
        }

        if (cameraHexY > wrap.y) {
            cameraHexY -= wrap.y * 2;
            wrappedY = htw_geo_hexToCartesianPositionY(cameraHexY);
            // x position is dependent on y position when converting between hex and cartesian space, so also need to assign back to cameraX
            wrappedX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
        } else if (cameraHexY < -wrap.y) {
            cameraHexY += wrap.y * 2;
            wrappedY = htw_geo_hexToCartesianPositionY(cameraHexY);
            wrappedX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
        }
    }

    cam->origin = (vec3){{wrappedX, wrappedY, cam->origin.z + camDelta.z}};
}

void CameraOrbitMouse(ecs_iter_t *it) {
    Camera *cam = ecs_field(it, Camera, 1);
    Pointer *pointer = ecs_field(it, Pointer, 2);

    float dT = it->delta_time;
    float speed = powf(cam->distance, 2.0) * dT * 0.1;
    if (ecs_field_is_set(it, 3)) {
        CameraSpeed camSpeed = *ecs_field(it, CameraSpeed, 3);
        speed *= camSpeed.movement;
    }
    float pitch = (pointer->y - pointer->lastY) * speed;
    float yaw = (pointer->x - pointer->lastX) * speed * -1.0; // flip sign to match expected movement direction

    cam->pitch = CLAMP(cam->pitch + pitch, -89.9, 89.9);
    cam->yaw += yaw;
}

void CameraZoomMouse(ecs_iter_t *it) {

}

void BcviewSystemsInputImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewSystemsInput);

    ECS_IMPORT(world, Bcview);

    ECS_SYSTEM(world, LockMouse, 0);

    ECS_SYSTEM(world, UnlockMouse, 0);

    ECS_SYSTEM(world, CameraPanMouse, 0,
        Camera($),
        Pointer($),
        ?CameraSpeed(Pointer),
        ?CameraWrap($)
    );

    ECS_SYSTEM(world, CameraOrbitMouse, 0,
        Camera($),
        Pointer($),
        ?CameraSpeed(Pointer)
    );

    ECS_SYSTEM(world, CameraZoomMouse, 0,
        Camera($),
        Pointer($),
        ?CameraSpeed(Pointer)
    );
}
