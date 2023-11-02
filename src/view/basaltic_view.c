#include "basaltic_view.h"
#include "basaltic_window.h"
#include "basaltic_sokol_gfx.h"
#include "basaltic_editor.h"
#include "basaltic_interaction.h"
#include "basaltic_uiState.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "basaltic_systems_view.h"
#include "flecs.h"
#include "bc_flecs_utils.h"

typedef struct {
    ecs_world_t *ecsWorld;
    // TODO: change the scope of render context to be much smaller or remove entirely
    bc_UiState *ui;
} bc_ViewContext;

static bc_ViewContext vc;

void bc_view_setup(bc_WindowContext* wc) {
    bc_sg_setup();

    vc.ecsWorld = ecs_init();

    // TEST: enable rest api
    //ecs_singleton_set(vc.ecsWorld, EcsRest, {0});

    ECS_IMPORT(vc.ecsWorld, Bcview);
    ECS_IMPORT(vc.ecsWorld, BcviewPhases);
    // TODO: script initialization method for ECS worlds to apply all scripts in a directory at startup
    ecs_plecs_from_file(vc.ecsWorld, "view/plecs/startup/startup_test.flecs");
    ecs_set_pair(vc.ecsWorld, 0, ResourceFile, FlecsScriptSource, {.path = "view/plecs/test.flecs"});

    // TEST: disable until I get WebGL shaders working
    //ECS_IMPORT(vc.ecsWorld, BcviewSystems);

    ecs_singleton_set(vc.ecsWorld, WindowSize, {.x = wc->width, .y = wc->height});

    vc.ui = bc_createUiState();
}

void bc_view_teardown() {
    sg_shutdown();
    ecs_fini(vc.ecsWorld);
}

void bc_view_beginFrame(bc_WindowContext* wc) {
    sg_pass_action pa = { 0
        //.colors[0] = { .action=SG_ACTION_CLEAR, .value={1.0f, 0.0f, 0.0f, 1.0f} }
    };
}

void bc_view_endFrame(bc_WindowContext* wc) {
    sg_commit();
}

void bc_view_onInputEvent(bc_CommandBuffer inputBuffer, SDL_Event *e, bool useMouse, bool useKeyboard) {
    bc_processInputEvent(vc.ecsWorld, inputBuffer, e, useMouse, useKeyboard);
}

void bc_view_processInputState(bc_CommandBuffer inputBuffer, bool useMouse, bool useKeyboard) {
    bc_processInputState(vc.ecsWorld, inputBuffer, useMouse, useKeyboard);
}

u32 bc_view_drawFrame(bc_SupervisorInterface* si, bc_ModelData* model, bc_WindowContext* wc, bc_CommandBuffer inputBuffer) {
    // TODO: should happen in response to SDL resize events instead of every frame
    const WindowSize *currentWindow = ecs_singleton_get(vc.ecsWorld, WindowSize);
    // Only set if changed so that OnSet observers for WindowSize only run when needed
    if (currentWindow->x != wc->width || currentWindow->y != wc->height) {
        ecs_singleton_set(vc.ecsWorld, WindowSize, {.x = wc->width, .y = wc->height});
    }

    bc_WorldState *world = model == NULL ? NULL : model->world;

    float dT = (float)wc->lastFrameDuration / wc->performanceFrequency; // in seconds
    ecs_singleton_set(vc.ecsWorld, DeltaTime, {dT});
    ecs_singleton_set(vc.ecsWorld, Clock, {(float)wc->milliSeconds / 1000.0});

    // TEST: reload test script
    bc_reloadFlecsScript(vc.ecsWorld, 0);

    ecs_progress(vc.ecsWorld, dT);

    if (world != NULL) {
        ModelWorld *mw = ecs_singleton_get_mut(vc.ecsWorld, ModelWorld);
        bool stepChanged = mw->lastRenderedStep < world->step;
        if (stepChanged || mw->renderOutdated) {
            if (SDL_SemWaitTimeout(world->lock, 4) != SDL_MUTEX_TIMEDOUT) {
                // set model ecs world scope, to keep view's external tags/queries separate
                ecs_entity_t oldScope = ecs_get_scope(world->ecsWorld);
                ecs_entity_t viewScope = ecs_entity_init(world->ecsWorld, &(ecs_entity_desc_t){.name = "bcview"});
                ecs_set_scope(world->ecsWorld, viewScope);
                // Only safe to iterate model queries while the world is in readonly mode, or has exclusive access from one thread
                // TODO: could be useful to pass elapsed model steps as delta time
                ecs_run_pipeline(vc.ecsWorld, ModelChangedPipeline, 1.0f);
                ecs_set_scope(world->ecsWorld, oldScope);
                SDL_SemPost(world->lock);
            }
        }
        mw->lastRenderedStep = world->step;
        mw->renderOutdated = false;
        ecs_singleton_modified(vc.ecsWorld, ModelWorld);
    }

    // TODO: return elapsed time in ms
    return 0;
}

void bc_view_onModelStart(bc_ModelData *model) {
    ecs_singleton_set(vc.ecsWorld, ModelWorld, {.world = model->world->ecsWorld, .lastRenderedStep = 0, .renderOutdated = false});
    ecs_singleton_set(vc.ecsWorld, FocusPlane, {model->world->centralPlane});

    // TEST: ensure that import order hasn't caused mismatched component IDs
    ecs_entity_t modelPlaneId = ecs_lookup_fullpath(model->world->ecsWorld, "basaltic.components.planes.Plane");
    ecs_entity_t viewPlaneId = ecs_lookup_fullpath(vc.ecsWorld, "basaltic.components.planes.Plane"); //ecs_id(Plane);
    assert(modelPlaneId == viewPlaneId);

    bc_setCameraWrapLimits(vc.ecsWorld);

    ecs_run_pipeline(vc.ecsWorld, ModelChangedPipeline, 1.0f);

    bc_editorOnModelStart();
}

void bc_view_onModelStop() {
    ecs_singleton_remove(vc.ecsWorld, ModelWorld);

    bc_editorOnModelStop();
}

void bc_view_setupEditor() {
    // TODO: can handle renderer-specific imgui setup here
    bc_setupEditor();
}

void bc_view_teardownEditor() {
    bc_teardownEditor();
}

void bc_view_drawEditor(bc_SupervisorInterface* si, bc_ModelData* model, bc_CommandBuffer inputBuffer) {
    bc_drawEditor(si, model, inputBuffer, vc.ecsWorld, vc.ui);
}
