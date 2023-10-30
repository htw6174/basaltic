#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "htw_core.h"
#include "htw_geomap.h"
#include "bc_flecs_utils.h"
#include "basaltic_interaction.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_logic.h"
#include "basaltic_components_view.h"
#include "components/basaltic_components_planes.h"

static void setCameraWrapLimits(bc_UiState *ui, u32 worldGridSizeX, u32 worldGridSizeY);
static void translateCamera(ecs_world_t *world, Camera camDelta);
static void selectCell(ecs_world_t *world);
static void advanceStep(bc_CommandBuffer commandBuffer);
static void playStep(bc_CommandBuffer commandBuffer);
static void pauseStep(bc_CommandBuffer commandBuffer);

void editTerrain(ecs_world_t *world, float strength);
void spawnPrefab(ecs_world_t *world);

// TODO: add seperate input handling for each interfaceMode setting
void bc_processInputEvent(ecs_world_t *world, bc_CommandBuffer commandBuffer, SDL_Event *e, bool useMouse, bool useKeyboard) {
    if (useMouse && e->type == SDL_MOUSEBUTTONDOWN) {
        if (e->button.button == SDL_BUTTON_LEFT) {
            //editTerrain(world, 1.0);
            spawnPrefab(world);
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            //TODO: moveCharacter();
            //advanceStep(commandBuffer);
            editTerrain(world, -1.0);
        } else if (e->button.button == SDL_BUTTON_MIDDLE) {
            selectCell(world);
        }
    }
    if (useKeyboard && e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_p:
                //ui->cameraMode = ui->cameraMode ^ 1; // invert
                playStep(commandBuffer);
                break;
            case SDLK_c:
                //bc_snapCameraToCharacter(ui, ui->activeCharacter);
                break;
            case SDLK_UP:
                editTerrain(world, 1.0);
                break;
            case SDLK_DOWN:
                editTerrain(world, -1.0);
                break;
            case SDLK_LEFT:
            {
                TerrainBrush *tb = ecs_singleton_get_mut(world, TerrainBrush);
                tb->value -= 1;
                ecs_singleton_modified(world, TerrainBrush);
                break;
            }
            case SDLK_RIGHT:
            {
                TerrainBrush *tb = ecs_singleton_get_mut(world, TerrainBrush);
                tb->value += 1;
                ecs_singleton_modified(world, TerrainBrush);
                break;
            }
            case SDLK_SPACE:
                advanceStep(commandBuffer);
                pauseStep(commandBuffer);
                break;
        }
    }
}

void bc_processInputState(ecs_world_t *world, bc_CommandBuffer commandBuffer, bool useMouse, bool useKeyboard) {
    float dT = ecs_singleton_get(world, DeltaTime)->seconds;

    if (useMouse) {
        // get mouse state
        s32 x, y, deltaX, deltaY;
        Uint32 mouseStateMask = SDL_GetMouseState(&x, &y);
        const Pointer *p = ecs_singleton_get(world, Pointer);

        deltaX = x - p->x;
        deltaY = y - p->y;
        bool mouseMoved = deltaX != 0 || deltaY != 0;

        const HoveredCell *currentHover = ecs_singleton_get(world, HoveredCell);
        const HoveredCell *prevHover = ecs_get_pair_second(world, ecs_id(Pointer), Previous, HoveredCell);

        bool hoverChanged = currentHover->x != prevHover->x || currentHover->y != prevHover->y;

        // Should only do this if mouse also moved this frame; prevents jittering on cell edges, and unintentional edits where lowering terrain changes the hovered cell
        if (mouseMoved && hoverChanged) {
            // Hovered cell changed
            // TODO: onClick and onHoverChanged should usually be connected; make mouse actions configurable or context-dependent
            if (mouseStateMask & SDL_BUTTON_LMASK) {
                editTerrain(world, 1.0);
            }
            if (mouseStateMask & SDL_BUTTON_RMASK) {
                editTerrain(world, -1.0);
            }
        }

        ecs_singleton_set(world, Pointer, {x, y, p->x, p->y});
        ecs_set_pair_second(world, ecs_id(Pointer), Previous, HoveredCell, {currentHover->x, currentHover->y});
    }

    // camera
    const Camera *cam = ecs_singleton_get(world, Camera);
    const CameraSpeed *camSpeed = ecs_singleton_get(world, CameraSpeed);
    Camera camDelta = {0};
    if (useKeyboard) {
        // handle continuous button presses
        const Uint8 *k = SDL_GetKeyboardState(NULL);
        if (k[SDL_SCANCODE_A]) camDelta.origin.x -= camSpeed->movement * powf(cam->distance, 2.0) * dT * 0.1;
        if (k[SDL_SCANCODE_D]) camDelta.origin.x += camSpeed->movement * powf(cam->distance, 2.0) * dT * 0.1;
        if (k[SDL_SCANCODE_W]) camDelta.origin.y += camSpeed->movement * powf(cam->distance, 2.0) * dT * 0.1;
        if (k[SDL_SCANCODE_S]) camDelta.origin.y -= camSpeed->movement * powf(cam->distance, 2.0) * dT * 0.1;
        if (k[SDL_SCANCODE_Q]) camDelta.yaw -= camSpeed->rotation * dT;
        if (k[SDL_SCANCODE_E]) camDelta.yaw += camSpeed->rotation * dT;
        if (k[SDL_SCANCODE_R]) camDelta.pitch += camSpeed->rotation * dT;
        if (k[SDL_SCANCODE_F]) camDelta.pitch -= camSpeed->rotation * dT;
        if (k[SDL_SCANCODE_T]) camDelta.origin.z += camSpeed->movement * dT;
        if (k[SDL_SCANCODE_G]) camDelta.origin.z -= camSpeed->movement * dT;
        if (k[SDL_SCANCODE_Z]) camDelta.distance += camSpeed->movement * dT;
        if (k[SDL_SCANCODE_X]) camDelta.distance -= camSpeed->movement * dT;
    }

    translateCamera(world, camDelta);
}

// TODO: should put all of this in an observer of focusplane changes
void bc_setCameraWrapLimits(ecs_world_t *world) {
    ecs_world_t *modelWorld = ecs_singleton_get(world, ModelWorld)->world;
    ecs_entity_t focus = ecs_singleton_get(world, FocusPlane)->entity;
    htw_ChunkMap *cm = ecs_get(modelWorld, focus, Plane)->chunkMap;
    ecs_singleton_set(world, CameraWrap, {
        .x = cm->mapWidth / 2,
        .y = cm->mapHeight / 2
    });
    {
        float x00, y00;
        htw_geo_getHexCellPositionSkewed((htw_geo_GridCoord){-cm->mapWidth, -cm->mapHeight}, &x00, &y00);
        float x10 = htw_geo_getHexPositionX(0, -cm->mapHeight);
        float x01 = htw_geo_getHexPositionX(-cm->mapWidth, 0);
        float x11 = 0.0;
        float y10 = y00;
        float y01 = 0.0;
        float y11 = 0.0;
        ecs_singleton_set(world, WrapInstanceOffsets, {
            .offsets[0] = {{x00, y00, 0.0}},
            .offsets[1] = {{x01, y01, 0.0}},
            .offsets[2] = {{x10, y10, 0.0}},
            .offsets[3] = {{x11, y11, 0.0}}
        });
    }
}

void bc_focusCameraOnCell(bc_UiState *ui, htw_geo_GridCoord cellCoord) {
    //htw_geo_getHexCellPositionSkewed(cellCoord, &ui->cameraX, &ui->cameraY);
}

void bc_snapCameraToCharacter(bc_UiState *ui, ecs_entity_t e) {
    // TODO: make this work with ecs. Should fix the other todo as well
    //if (subject == NULL) return;
    //htw_geo_GridCoord characterCoord = subject->currentState.worldCoord;
    //bc_focusCameraOnCell(ui, characterCoord);
    // TODO: would like to set camera height also, but that requires inspecting world data as well. Maybe setup a general purpose height adjust in bc_window
}

static void translateCamera(ecs_world_t *world, Camera camDelta) {
    Camera *cam = ecs_singleton_get_mut(world, Camera);
    const CameraWrap *wrap = ecs_singleton_get(world, CameraWrap);
    float yaw = cam->yaw * DEG_TO_RAD;
    float sinYaw = sin(yaw);
    float cosYaw = cos(yaw);
    float xGlobalMovement = (camDelta.origin.x * cosYaw) + (camDelta.origin.y * -sinYaw);
    float yGlobalMovement = (camDelta.origin.y * cosYaw) + (camDelta.origin.x * sinYaw);

    float unwrappedX = cam->origin.x + xGlobalMovement;
    float unwrappedY = cam->origin.y + yGlobalMovement;
    float wrappedX = unwrappedX;
    float wrappedY = unwrappedY;

    // Wrap camera if outsize wrap limit:
    // Convert camera cartesian coordinates to hex space (expands y)
    // Check against camera wrap bounds
    // If out of bounds: wrap in hex space, convert back to cartesian space, and assign back to camera
    float cameraHexX = htw_geo_cartesianToHexPositionX(unwrappedX, unwrappedY);
    float cameraHexY = htw_geo_cartesianToHexPositionY(unwrappedY);
    if (cameraHexX > wrap->x) {
        cameraHexX -= wrap->x * 2;
        wrappedX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
    } else if (cameraHexX < -wrap->x) {
        cameraHexX += wrap->x * 2;
        wrappedX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
    }

    if (cameraHexY > wrap->y) {
        cameraHexY -= wrap->y * 2;
        wrappedY = htw_geo_hexToCartesianPositionY(cameraHexY);
        // x position is dependent on y position when converting between hex and cartesian space, so also need to assign back to cameraX
        wrappedX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
    } else if (cameraHexY < -wrap->y) {
        cameraHexY += wrap->y * 2;
        wrappedY = htw_geo_hexToCartesianPositionY(cameraHexY);
        wrappedX = htw_geo_hexToCartesianPositionX(cameraHexX, cameraHexY);
    }

    cam->origin = (vec3){{wrappedX, wrappedY, cam->origin.z + camDelta.origin.z}};
    cam->distance += camDelta.distance;
    cam->pitch += camDelta.pitch;
    cam->yaw += camDelta.yaw;

    ecs_singleton_modified(world, Camera);
}

static void selectCell(ecs_world_t *world) {
    const HoveredCell *hovered = ecs_singleton_get(world, HoveredCell);
    ecs_singleton_set(world, SelectedCell, {.x = hovered->x, .y = hovered->y});
}

static void advanceStep(bc_CommandBuffer commandBuffer) {
    bc_WorldCommand stepCommand = {
        .commandType = BC_COMMAND_TYPE_STEP_ADVANCE,
    };
    bc_pushCommandToBuffer(commandBuffer, &stepCommand, sizeof(stepCommand));
}

static void playStep(bc_CommandBuffer commandBuffer) {
    bc_WorldCommand stepCommand = {
        .commandType = BC_COMMAND_TYPE_STEP_PLAY,
    };
    bc_pushCommandToBuffer(commandBuffer, &stepCommand, sizeof(stepCommand));
}

static void pauseStep(bc_CommandBuffer commandBuffer) {
    bc_WorldCommand stepCommand = {
        .commandType = BC_COMMAND_TYPE_STEP_PAUSE,
    };
    bc_pushCommandToBuffer(commandBuffer, &stepCommand, sizeof(stepCommand));
}

void editTerrain(ecs_world_t *world, float strength) {
    const TerrainBrush *tb = ecs_singleton_get(world, TerrainBrush);
    const HoveredCell *hoveredCoord = ecs_singleton_get(world, HoveredCell);
    htw_geo_GridCoord cellCoord = *(htw_geo_GridCoord*)hoveredCoord;

    const FocusPlane *fp = ecs_singleton_get(world, FocusPlane);
    const ModelWorld *mw = ecs_singleton_get(world, ModelWorld);

    // TODO: also check to see if world is locked, don't edit if so
    if (tb && hoveredCoord && fp && mw) {
        ecs_entity_t focusPlane = fp->entity;
        ecs_world_t *modelWorld = mw->world;
        htw_ChunkMap *cm = ecs_get(modelWorld, focusPlane, Plane)->chunkMap;

        u32 area = htw_geo_getHexArea(tb->radius);
        htw_geo_GridCoord offsetCoord = {0, 0};
        for (int i = 0; i < area; i++) {
            CellData *cd = htw_geo_getCell(cm, htw_geo_addGridCoords(cellCoord, offsetCoord));
            cd->height += tb->value * strength;
            htw_geo_CubeCoord cubeOffset = htw_geo_gridToCubeCoord(offsetCoord);
            htw_geo_getNextHexSpiralCoord(&cubeOffset);
            offsetCoord = htw_geo_cubeToGridCoord(cubeOffset);
        }

        // Mark chunk dirty so it can be rebuilt TODO: will need to to exactly once for each unique chunk modified by a brush
        // DirtyChunkBuffer *dirty = ecs_singleton_get_mut(world, DirtyChunkBuffer);
        // u32 chunk = htw_geo_getChunkIndexByGridCoordinates(cm, cellCoord);
        // dirty->chunks[dirty->count++] = chunk;
        // ecs_singleton_modified(world, DirtyChunkBuffer);

        bc_redraw_model(world);
    }
}

void spawnPrefab(ecs_world_t *world) {
    const PrefabBrush *pb = ecs_singleton_get(world, PrefabBrush);
    const HoveredCell *hoveredCoord = ecs_singleton_get(world, HoveredCell);
    htw_geo_GridCoord cellCoord = *(htw_geo_GridCoord*)hoveredCoord;

    const FocusPlane *fp = ecs_singleton_get(world, FocusPlane);
    const ModelWorld *mw = ecs_singleton_get(world, ModelWorld);

    // TODO: also check to see if world is locked, don't edit if so
    if (pb && hoveredCoord && fp && mw) {
        ecs_entity_t focusPlane = fp->entity;
        ecs_world_t *modelWorld = mw->world;
        htw_ChunkMap *cm = ecs_get(modelWorld, focusPlane, Plane)->chunkMap;
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

        bc_redraw_model(world);
    }
}
