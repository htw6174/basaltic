#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "bc_sdl_utils.h"
#include "htw_core.h"
#include "htw_geomap.h"
#include "bc_flecs_utils.h"
#include "basaltic_interaction.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_logic.h"
#include "basaltic_components_view.h"
#include "components/basaltic_components_planes.h"

static ecs_query_t *bindingsQuery;

static void setCameraWrapLimits(bc_UiState *ui, u32 worldGridSizeX, u32 worldGridSizeY);
static void advanceStep(bc_CommandBuffer commandBuffer);
static void playStep(bc_CommandBuffer commandBuffer);
static void pauseStep(bc_CommandBuffer commandBuffer);

// FIXME: can't combine operators, so no way to have (!Disabled || ActiveBindGroup). Need different solution
ecs_query_t *createBindingsQuery(ecs_world_t *world) {
    return ecs_query_init(world, &(ecs_query_desc_t){
        //.filter.expr = "[in] InputBinding, !Disabled(parent) || bcview.ActiveBindGroup(parent)"
        .filter.terms = {
            [0] = {.inout = EcsIn, .id = ecs_id(InputBinding)},
            // Don't use bindings whose parents are disabled, so groups of bindings can be toggled together
            [1] = {.inout = EcsInOutNone, .id = EcsDisabled, .src.flags = EcsParent, .oper = EcsNot}
            // Override for otherwise disabled bindgroups
            // [2] = {.inout = EcsInOutNone, .id = ActiveBindGroup, .src.flags = EcsParent, .oper = EcsOptional},
        }
    });
}

// TODO: add seperate input handling for each interfaceMode setting
void bc_processInputEvent(ecs_world_t *world, bc_CommandBuffer commandBuffer, SDL_Event *e, bool useMouse, bool useKeyboard) {
    float dT = ecs_singleton_get(world, DeltaTime)->seconds;
    const WindowSize *windowSize = ecs_singleton_get(world, WindowSize);
    const MousePreferences *mousePreferences = ecs_singleton_get(world, MousePreferences);

    if (bindingsQuery == NULL) {
        bindingsQuery = createBindingsQuery(world);
    }

    ecs_iter_t it = ecs_query_iter(world, bindingsQuery);
    while (ecs_query_next(&it)) {
        InputBinding *bindings = ecs_field(&it, InputBinding, 1);

        for (int i = 0; i < it.count; i++) {
            InputBinding bind = bindings[i];
            vec2 delta = bind.axis; // Default to binding axis, override with motion if set
            bool match = false; // Does this event satisfy the binding requirements?
            if (bind.system == 0) {
                continue;
            }
            // First, determine what kind of event(s) the binding should trigger on
            if (useKeyboard && bind.key) {
                // keydown or keyup
                InputType keyInputType = (e->type == SDL_KEYDOWN) | ((e->type == SDL_KEYUP) << 1);
                if (keyInputType) {
                    // type must match at least one triggerOn type i.e. at least one bit must match
                    InputType triggerOn = bind.triggerOn ? bind.triggerOn : BC_INPUT_PRESSED; // default to pressed
                    if ((triggerOn & keyInputType) && (e->key.keysym.sym == bind.key)) {
                        match = true;
                    }
                }
            }
            if (useMouse) {
                if (bind.button) {
                    // mousebuttondown or mousebuttonup
                    InputType mouseInputType = (e->type == SDL_MOUSEBUTTONDOWN) | ((e->type == SDL_MOUSEBUTTONUP) << 1);
                    if (mouseInputType) {
                        // type must match at least one triggerOn type i.e. at least one bit must match
                        InputType triggerOn = bind.triggerOn ? bind.triggerOn : BC_INPUT_PRESSED; // default to pressed
                        if ((triggerOn & mouseInputType) && (e->button.button == bind.button)) {
                            match = true;
                        }
                    }
                }
                if (bind.motion == BC_MOTION_MOUSE) {
                    if (e->type == SDL_MOUSEMOTION) {
                        // Normalize mouse motion by window width
                        float dX = ((float)e->motion.xrel * mousePreferences->sensitivity * (mousePreferences->invertX ? -1.0 : 1.0)) / windowSize->x;
                        float dY = ((float)e->motion.yrel * mousePreferences->sensitivity * mousePreferences->verticalSensitivity * (mousePreferences->invertY ? -1.0 : 1.0)) / windowSize->x;
                        delta = (vec2){{dX, dY}};
                        // if button is set, must check state for matching mask
                        if (bind.button) {
                            match = SDL_BUTTON(bind.button) & e->motion.state;
                        } else {
                            match = true;
                        }
                    } else {
                        match = false;
                    }
                } else if (bind.motion == BC_MOTION_SCROLL) {
                    if (e->type == SDL_MOUSEWHEEL) {
                        delta = (vec2){{e->wheel.x * mousePreferences->scrollSensitivity,
                                        e->wheel.y * mousePreferences->scrollSensitivity}};
                        // TODO: could add a buttonpressed requirement along with this but no need to right now
                        match = true;
                    } else {
                        match =false;
                    }
                } else if (bind.motion == BC_MOTION_TILE) {
                    // userevent TILEMOTION pushed back into SDL
                    if (e->type == BC_SDL_USEREVENT_TYPE(BC_SDL_TILEMOTION)) {
                        delta = (vec2){{e->motion.xrel, e->motion.yrel}};
                        // if button is set, must check state for matching mask
                        if (bind.button) {
                            match = SDL_BUTTON(bind.button) & e->motion.state;
                        } else {
                            match = true;
                        }
                    } else {
                        match = false;
                    }
                }
            }

            if (match) {
                InputContext ic = {
                    .axis = bind.axis,
                    .delta = delta
                };
                ecs_run(world, bind.system, dT, &ic);
            }
        }
    }

    // TODO: create input systems for these so they can be remapped
    if (useKeyboard && e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_p:
                //ui->cameraMode = ui->cameraMode ^ 1; // invert
                playStep(commandBuffer);
                break;
            case SDLK_SPACE:
                advanceStep(commandBuffer);
                pauseStep(commandBuffer);
                break;
        }
    }
}

void bc_processInputState(ecs_world_t *world, bc_CommandBuffer commandBuffer, bool useMouse, bool useKeyboard) {
    float dT = ecs_singleton_get(world, DeltaTime)->seconds;
    const Pointer *p = ecs_singleton_get(world, Pointer);
    const HoveredCell *currentHover = ecs_singleton_get(world, HoveredCell);
    const HoveredCell *prevHover = ecs_get_pair_second(world, ecs_id(HoveredCell), Previous, HoveredCell);

    // get mouse state
    s32 x, y, deltaX, deltaY, hoverDeltaX, hoverDeltaY;
    Uint32 mouseStateMask = SDL_GetMouseState(&x, &y);
    deltaX = x - p->x;
    deltaY = y - p->y;
    hoverDeltaX = currentHover->x - prevHover->x;
    hoverDeltaY = currentHover->y - prevHover->y;
    bool hoverChanged = hoverDeltaX != 0 || hoverDeltaY != 0;
    if (hoverChanged) {
        SDL_Event tileMotionEvent = {
            .motion = {
                .type = BC_SDL_USEREVENT_TYPE(BC_SDL_TILEMOTION),
                .timestamp = SDL_GetTicks(),
                .state = mouseStateMask,
                .x = currentHover->x,
                .y = currentHover->y,
                .xrel = hoverDeltaX,
                .yrel = hoverDeltaY
            }
        };
        SDL_PushEvent(&tileMotionEvent);
    }

    const Uint8 *keyboard = SDL_GetKeyboardState(NULL);

    if (bindingsQuery == NULL) {
        bindingsQuery = createBindingsQuery(world);
    }

    ecs_iter_t it = ecs_query_iter(world, bindingsQuery);
    while (ecs_query_next(&it)) {
        InputBinding *bindings = ecs_field(&it, InputBinding, 1);

        for (int i = 0; i < it.count; i++) {
            InputBinding bind = bindings[i];
            if (bind.system == 0) {
                continue;
            }
            // Only process input held triggers here
            if ((bind.triggerOn & BC_INPUT_HELD) == 0) {
                continue;
            }
            if (bind.button) {
                // button must be one of the mouse buttons pressed this frame
                bool buttonMatch = SDL_BUTTON(bind.button) & mouseStateMask;
                if (!useMouse || !buttonMatch) {
                    continue;
                }
            }
            if (bind.key) {
                bool keyMatch = keyboard[SDL_GetScancodeFromKey((SDL_KeyCode)bind.key)];
                if (!useKeyboard || !keyMatch) {
                    continue;
                }
            }
            InputContext ic = {
                .delta = bind.axis
            };
            ecs_run(world, bind.system, dT, &ic);
        }
    }

    if (useMouse) {
        ecs_singleton_set(world, Pointer, {x, y, p->x, p->y});
        ecs_set_pair_second(world, ecs_id(HoveredCell), Previous, HoveredCell, {currentHover->x, currentHover->y});
    }
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
