#include "basaltic_view.h"
#include "basaltic_window.h"
#include "basaltic_sokol_gfx.h"
// #include "basaltic_editor.h"
#include "systems/systems_input.h"
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
static bc_ModelContext *model;

void bc_view_setup(bc_WindowContext* wc) {
    bc_sg_setup();

    vc.ecsWorld = ecs_init();

    // TEST: enable rest api
    //ecs_singleton_set(vc.ecsWorld, EcsRest, {0});

    ECS_IMPORT(vc.ecsWorld, Bcview);
    ECS_IMPORT(vc.ecsWorld, BcviewPhases);
    ECS_IMPORT(vc.ecsWorld, BcviewSystems);
    // TODO: script initialization method for ECS worlds to apply all scripts in a directory at startup
    ecs_plecs_from_file(vc.ecsWorld, "view/plecs/startup/startup_test.flecs");

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

void bc_view_onInputEvent(SDL_Event *e, bool useMouse, bool useKeyboard) {
    SystemsInput_ProcessEvent(vc.ecsWorld, e);
}

void bc_view_processInputState(bool useMouse, bool useKeyboard) {
    // TODO: still needed?
}

u32 bc_view_drawFrame(bc_SupervisorInterface *si, bc_WindowContext *wc) {
    // TODO: should happen in response to SDL resize events instead of every frame
    const WindowSize *currentWindow = ecs_singleton_get(vc.ecsWorld, WindowSize);
    // Only set if changed so that OnSet observers for WindowSize only run when needed
    if (currentWindow->x != wc->width || currentWindow->y != wc->height) {
        ecs_singleton_set(vc.ecsWorld, WindowSize, {.x = wc->width, .y = wc->height});
    }

    float dT = (float)wc->lastFrameDuration / wc->performanceFrequency; // in seconds
    ecs_singleton_set(vc.ecsWorld, DeltaTime, {dT});
    ecs_singleton_set(vc.ecsWorld, Clock, {(float)wc->milliSeconds / 1000.0});

    ecs_progress(vc.ecsWorld, dT);

    ecs_world_t *world = model == NULL ? NULL : model->world;
    if (world != NULL) {
        ModelWorld *mw = ecs_singleton_get_mut(vc.ecsWorld, ModelWorld);
        bool stepChanged = mw->lastRenderedStep < model->step;
        if (stepChanged || mw->renderOutdated) {
            if (SDL_TryLockMutex(model->mutex) == 0) {
                // set model ecs world scope, to keep view's external tags/queries separate
                ecs_entity_t viewScope = ecs_entity_init(world, &(ecs_entity_desc_t){.name = "bcview"});
                ecs_entity_t oldScope = ecs_set_scope(world, viewScope);
                // Only safe to iterate model queries while the world is in readonly mode, or has exclusive access from one thread
                // TODO: could be useful to pass elapsed model steps as delta time
                ecs_run_pipeline(vc.ecsWorld, ModelChangedPipeline, 1.0f);
                ecs_set_scope(world, oldScope);
                SDL_UnlockMutex(model->mutex);
            }
        }
        mw->lastRenderedStep = model->step;
        mw->renderOutdated = false;
        ecs_singleton_modified(vc.ecsWorld, ModelWorld);

        // Queue up model steps to run
        ModelStepControl *stepper = ecs_singleton_get_mut(vc.ecsWorld, ModelStepControl);
        model->runForSteps = stepper->stepsPerRun;
        if (stepper->doSingleRun) {
            stepper->doSingleRun = false;
            ecs_singleton_modified(vc.ecsWorld, ModelStepControl);
            SDL_CondSignal(model->cond);
        } else if (stepper->doAuto) {
            if (wc->frame % stepper->framesBetweenRuns == 0) {
                SDL_CondSignal(model->cond);
            }
        }
    }

    // TODO: return elapsed time in ms
    return 0;
}

void bc_view_onModelStart(bc_ModelContext *mctx) {
    model = mctx;
    ecs_singleton_set(vc.ecsWorld, ModelWorld, {.world = model->world, .lastRenderedStep = 0, .renderOutdated = true});
    // Find first Plane entity in model to use a default focus
    ecs_entity_t modelPlaneId = ecs_lookup_fullpath(model->world, "bc.planes.Plane");
    ecs_iter_t planes = ecs_term_iter(model->world, &(ecs_term_t){
        .id = modelPlaneId
    });
    ecs_entity_t firstPlane = ecs_iter_first(&planes);
    ecs_singleton_set(vc.ecsWorld, FocusPlane, {firstPlane});

    // TEST: ensure that import order hasn't caused mismatched component IDs
    ecs_entity_t viewPlaneId = ecs_lookup_fullpath(vc.ecsWorld, "bc.planes.Plane"); //ecs_id(Plane);
    assert(modelPlaneId == viewPlaneId);

    // bc_editorOnModelStart();

    // Progress single step to run EcsOnStart systems
    SDL_CondSignal(mctx->cond);
}

void bc_view_onModelStop(bc_ModelContext *mctx) {
    ecs_singleton_remove(vc.ecsWorld, ModelWorld);
    // bc_editorOnModelStop();
    model = NULL;
}

void bc_view_setupEditor() {
    // TODO: can handle renderer-specific imgui setup here
    // bc_setupEditor();
}

void bc_view_teardownEditor() {
    // bc_teardownEditor();
}

void bc_view_drawEditor(bc_SupervisorInterface *si) {
    // bc_drawEditor(si, model, vc.ecsWorld, vc.ui);
}

void bc_view_drawGUI(bc_SupervisorInterface* si) {
    // bc_drawGUI(si, model, vc.ecsWorld);
}
