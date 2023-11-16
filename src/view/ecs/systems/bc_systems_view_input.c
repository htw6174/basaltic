#include "bc_systems_view_input.h"
#include "basaltic_components_view.h"
#include "htw_core.h"
#include "bc_flecs_utils.h"
#include <math.h>
#include <SDL2/SDL.h>

// TODO: would like it if mouse didn't move from starting position between lock and unlock, or gets warped back to starting position on unlock
void LockMouse(ecs_iter_t *it) {
    SDL_SetRelativeMouseMode(true);
}

void UnlockMouse(ecs_iter_t *it) {
    SDL_SetRelativeMouseMode(false);
}

void CameraPan(ecs_iter_t *it) {
    Camera *cam = ecs_field(it, Camera, 1);

    float dT = it->delta_time;
    void *param = it->param;
    InputContext ic = param ? (*(InputContext*)param) : (InputContext){0};

    float speed = powf(cam->distance, 2.0) * dT;
    if (ecs_field_is_set(it, 3)) {
        CameraSpeed camSpeed = *ecs_field(it, CameraSpeed, 2);
        speed *= camSpeed.movement;
    }
    float dX = ic.delta.x * speed * -1.0; // flip sign to match expected movement direction
    float dY = ic.delta.y * speed;
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

    if (ecs_field_is_set(it, 3)) {
        CameraWrap wrap = *ecs_field(it, CameraWrap, 3);
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

void CameraOrbit(ecs_iter_t *it) {
    Camera *cam = ecs_field(it, Camera, 1);

    float dT = it->delta_time;
    void *param = it->param;
    InputContext ic = param ? (*(InputContext*)param) : (InputContext){0};

    float speed = dT;
    if (ecs_field_is_set(it, 2)) {
        CameraSpeed camSpeed = *ecs_field(it, CameraSpeed, 2);
        speed *= camSpeed.rotation;
    }
    float pitch = ic.delta.y * speed;
    float yaw = ic.delta.x * speed * -1.0; // flip sign to match expected movement direction

    cam->pitch = CLAMP(cam->pitch + pitch, -89.9, 89.9);
    cam->yaw += yaw;
}

void CameraZoom(ecs_iter_t *it) {
    Camera *cam = ecs_field(it, Camera, 1);

    float dT = it->delta_time;
    void *param = it->param;
    InputContext ic = param ? (*(InputContext*)param) : (InputContext){0};

    float speed = dT;
    if (ecs_field_is_set(it, 2)) {
        CameraSpeed camSpeed = *ecs_field(it, CameraSpeed, 2);
        speed *= camSpeed.zoom;
    }
    // positive y zooms in
    float delta = ic.delta.y * speed * -1.0;

    cam->distance = CLAMP(cam->distance + delta, 0.5, 100.0);
}

// TODO: unsure if this is the right module to put non-manual run systems in
void CameraDriftToElevation(ecs_iter_t *it) {
    Camera *cam = ecs_field(it, Camera, 1);

    float dT = it->delta_time;
    float targetElevation = 0.0; // drift to 0 if terrain elevation isn't available
    if (ecs_field_is_set(it, 2) && ecs_field_is_set(it, 3)) {
        FocusPlane *fp = ecs_field(it, FocusPlane, 2);
        ModelWorld *mw = ecs_field(it, ModelWorld, 3);
        const vec3 *scale = ecs_singleton_get(it->world, Scale);

        ecs_entity_t focusPlane = fp->entity;
        ecs_world_t *modelWorld = mw->world;
        htw_ChunkMap *cm = ecs_get(modelWorld, focusPlane, Plane)->chunkMap;

        htw_geo_GridCoord originCoord = htw_geo_cartesianToHexCoord(cam->origin.x, cam->origin.y);
        CellData *cd = htw_geo_getCell(cm, originCoord);
        targetElevation = (cd->height * scale->z) + scale->z; // target point just above surface
        // if within half an elevation step already, no need to move
        if (fabsf(cam->origin.z - targetElevation) < (scale->z / 2.0)) {
            return;
        }
    }
    targetElevation = MAX(targetElevation, 0.0); // don't sink into the ocean

    cam->origin.z = lerpClamp(cam->origin.z, targetElevation, 2.0 * dT);
}

void SelectCell(ecs_iter_t *it) {
    HoveredCell *hovered = ecs_field(it, HoveredCell, 1);
    SelectedCell *selected = ecs_field(it, SelectedCell, 2);
    selected->x = hovered->x;
    selected->y = hovered->y;
}

void SelectEntity(ecs_iter_t *it) {
    HoveredCell *hovered = ecs_field(it, HoveredCell, 1);
    FocusPlane *fp = ecs_field(it, FocusPlane, 2);
    ModelWorld *mw = ecs_field(it, ModelWorld, 3);
    FocusEntity *fe = ecs_field(it, FocusEntity, 4);

    // Use spatial storage to lookup root entity on hovered cell
    // TODO: later, GUI for focus entity will show details if an actor is selected, or a list of options if a CellRoot is selected
    fe->entity = plane_GetRootEntity(mw->world, fp->entity, (Position){hovered->x, hovered->y});
}

void CameraToPlayer(ecs_iter_t *it) {
    PlayerEntity *player = ecs_field(it, PlayerEntity, 1);
    ModelWorld *mw = ecs_field(it, ModelWorld, 2);
    Camera *cam = ecs_field(it, Camera, 3);

    ecs_world_t *world = mw->world;
    if (ecs_is_valid(world, player->entity)) {
        const Position *pos = ecs_get(world, player->entity, Position);
        if (pos != NULL) {
            float x, y;
            htw_geo_getHexCellPositionSkewed(*pos, &x, &y);
            // TODO: implement camera lerping, assign to lerp target instead of cam directly
            cam->origin.x = x;
            cam->origin.y = y;
            cam->distance = 2.0;
        }
    }
}

void SetPlayerDestination(ecs_iter_t *it) {
    PlayerEntity *player = ecs_field(it, PlayerEntity, 1);
    HoveredCell *hoveredCoord = ecs_field(it, HoveredCell, 2);
    FocusPlane *fp = ecs_field(it, FocusPlane, 3); // TODO: use this to set destination to a different plane
    ModelWorld *mw = ecs_field(it, ModelWorld, 4);

    ecs_world_t *world = mw->world;
    if (ecs_is_valid(world, player->entity)) {
        ecs_set(world, player->entity, Destination, {hoveredCoord->x, hoveredCoord->y});
        ecs_add_pair(world, player->entity, Action, ActionMove);
    }
}

void PaintPrefabBrush(ecs_iter_t *it) {
    PrefabBrush *pb = ecs_field(it, PrefabBrush, 1);
    HoveredCell *hoveredCoord = ecs_field(it, HoveredCell, 2);
    htw_geo_GridCoord cellCoord = *(htw_geo_GridCoord*)hoveredCoord;
    FocusPlane *fp = ecs_field(it, FocusPlane, 3);
    ModelWorld *mw = ecs_field(it, ModelWorld, 4);

    ecs_entity_t focusPlane = fp->entity;
    ecs_world_t *modelWorld = mw->world;
    Step step = *ecs_singleton_get(modelWorld, Step);

    ecs_entity_t e;
    if (pb->prefab != 0) {
        e = bc_instantiateRandomizer(modelWorld, pb->prefab);
    } else {
        e = ecs_new_w_pair(modelWorld, EcsChildOf, focusPlane);
    }
    ecs_add_pair(modelWorld, e, IsOn, focusPlane);
    ecs_set(modelWorld, e, Position, {cellCoord.x, cellCoord.y});
    ecs_set(modelWorld, e, CreationTime, {step});
    plane_PlaceEntity(modelWorld, focusPlane, e, cellCoord);

    bc_redraw_model(it->world);
}

void PaintAdditiveBrush(ecs_iter_t *it) {
    AdditiveBrush *ab = ecs_field(it, AdditiveBrush, 1);
    BrushSize *brushSize = ecs_field(it, BrushSize, 2);
    ecs_entity_t brushField = ECS_PAIR_SECOND(ecs_field_id(it, 3));
    HoveredCell *hoveredCoord = ecs_field(it, HoveredCell, 4);
    htw_geo_GridCoord cellCoord = *(htw_geo_GridCoord*)hoveredCoord;
    FocusPlane *fp = ecs_field(it, FocusPlane, 5);
    ModelWorld *mw = ecs_field(it, ModelWorld, 6);

    // Get field offset and type info
    const EcsMember *member = ecs_get(it->world, brushField, EcsMember);
    if (!member) {
        ecs_err("Target of BrushField has no member metadata: %s", ecs_get_name(it->world, brushField));
        return;
    }
    const EcsPrimitive *prim = ecs_get(it->world, member->type, EcsPrimitive);

    ptrdiff_t offset = member->offset;

    // Get axis data to multiply value by input strength
    void *param = it->param;
    InputContext ic = param ? (*(InputContext*)param) : (InputContext){.delta.x = 1.0};

    ecs_entity_t focusPlane = fp->entity;
    ecs_world_t *modelWorld = mw->world;
    htw_ChunkMap *cm = ecs_get(modelWorld, focusPlane, Plane)->chunkMap;

    u32 area = htw_geo_getHexArea(brushSize->radius);
    htw_geo_GridCoord offsetCoord = {0, 0};
    for (int i = 0; i < area; i++) {
        CellData *cd = htw_geo_getCell(cm, htw_geo_addGridCoords(cellCoord, offsetCoord));
        void *fieldPtr = ((void*)cd) + offset;

        // get value from cell by offset and primkind
        s64 value = bc_getMetaComponentMemberInt(fieldPtr, prim->kind);
        value += ab->value * ic.delta.x;
        bc_setMetaComponentMemberInt(fieldPtr, prim->kind, value);

        htw_geo_CubeCoord cubeOffset = htw_geo_gridToCubeCoord(offsetCoord);
        htw_geo_getNextHexSpiralCoord(&cubeOffset);
        offsetCoord = htw_geo_cubeToGridCoord(cubeOffset);
    }

    // Mark chunk dirty so it can be rebuilt TODO: will need to to exactly once for each unique chunk modified by a brush
    // DirtyChunkBuffer *dirty = ecs_singleton_get_mut(world, DirtyChunkBuffer);
    // u32 chunk = htw_geo_getChunkIndexByGridCoordinates(cm, cellCoord);
    // dirty->chunks[dirty->count++] = chunk;
    // ecs_singleton_modified(world, DirtyChunkBuffer);

    bc_redraw_model(it->world);
}

void BcviewSystemsInputImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewSystemsInput);

    ECS_IMPORT(world, Bcview);

    ECS_SYSTEM(world, LockMouse, 0);

    ECS_SYSTEM(world, UnlockMouse, 0);

    ECS_SYSTEM(world, CameraPan, 0,
        [inout] Camera($),
        ?CameraSpeed($),
        ?CameraWrap($)
    );

    ECS_SYSTEM(world, CameraOrbit, 0,
        [inout] Camera($),
        ?CameraSpeed($)
    );

    ECS_SYSTEM(world, CameraZoom, 0,
        [inout] Camera($),
        ?CameraSpeed($)
    );

    ECS_SYSTEM(world, CameraDriftToElevation, EcsPreUpdate,
        [inout] Camera($),
        ?FocusPlane($),
        ?ModelWorld($)
    );

    ECS_SYSTEM(world, SelectCell, 0,
        [in] HoveredCell($),
        [out] SelectedCell($)
    );

    ECS_SYSTEM(world, SelectEntity, 0,
        [in] HoveredCell($),
        [in] FocusPlane($),
        [in] ModelWorld($),
        [out] FocusEntity($)
    );

    ECS_SYSTEM(world, CameraToPlayer, 0,
        [in] PlayerEntity($),
        [in] ModelWorld($),
        [out] Camera($)
    );

    ECS_SYSTEM(world, SetPlayerDestination, 0,
        PlayerEntity($),
        HoveredCell($),
        FocusPlane($),
        ModelWorld($)
    );

    ECS_SYSTEM(world, PaintPrefabBrush, 0,
        PrefabBrush($),
        HoveredCell($),
        FocusPlane($),
        ModelWorld($)
    );

    ECS_SYSTEM(world, PaintAdditiveBrush, 0,
        AdditiveBrush($),
        BrushSize(AdditiveBrush),
        BrushField(AdditiveBrush, _),
        HoveredCell($),
        FocusPlane($),
        ModelWorld($)
    );
}
