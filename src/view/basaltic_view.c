#include "basaltic_view.h"
#include "basaltic_window.h"
#include "basaltic_editor.h"
#include "basaltic_interaction.h"
#include "basaltic_uiState.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "basaltic_systems_view.h"
#include "flecs.h"

#define SOKOL_IMPL
#define SOKOL_GLCORE33
#define SOKOL_NO_DEPRECATED
#ifdef DEBUG
#define SOKOL_DEBUG
#endif
#include "sokol/sokol_log.h"
#include "sokol/sokol_gfx.h"

typedef struct {
    ecs_world_t *ecsWorld;
    // TODO: change the scope of render context to be much smaller or remove entirely
    bc_UiState *ui;
} bc_ViewContext;

static bc_ViewContext vc;

void bc_view_setup(bc_WindowContext* wc) {
    sg_desc sgd = {
        .logger.func = slog_func,
    };
    sg_setup(&sgd);
    assert(sg_isvalid());

    vc.ecsWorld = ecs_init();
    ECS_IMPORT(vc.ecsWorld, Bcview);
    ECS_IMPORT(vc.ecsWorld, BcviewPhases);
    ECS_IMPORT(vc.ecsWorld, BcviewSystems);

    ecs_singleton_set(vc.ecsWorld, WindowSize, {.x = wc->width, .y = wc->height});

// #ifndef _WIN32
// // TODO: create a proper toggle for this in build settings
// #ifdef FLECS_REST
//     printf("Initializing flecs REST API\n");
//     ecs_singleton_set(vc.ecsWorld, EcsRest, {0});
//     //ECS_IMPORT(newWorld->ecsWorld, FlecsMonitor);
//     //FlecsMonitorImport(vc.ecsWorld);
//     ecs_set_scope(vc.ecsWorld, 0);
// #endif
// #endif

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
    sg_begin_default_pass(&pa, wc->width, wc->height);
}

void bc_view_endFrame(bc_WindowContext* wc) {
    sg_end_pass();
    sg_commit();
}

void bc_view_onInputEvent(bc_CommandBuffer inputBuffer, SDL_Event *e, bool useMouse, bool useKeyboard) {
    bc_processInputEvent(vc.ecsWorld, inputBuffer, e, useMouse, useKeyboard);
}

void bc_view_processInputState(bc_CommandBuffer inputBuffer, bool useMouse, bool useKeyboard) {
    bc_processInputState(vc.ecsWorld, inputBuffer, useMouse, useKeyboard);
}

u32 bc_view_drawFrame(bc_SupervisorInterface* si, bc_ModelData* model, bc_WindowContext* wc, bc_CommandBuffer inputBuffer) {
    ecs_singleton_set(vc.ecsWorld, WindowSize, {.x = wc->width, .y = wc->height});
    bc_WorldState *world = model == NULL ? NULL : model->world;

    float dT = (float)wc->lastFrameDuration / wc->performanceFrequency; // in seconds
    ecs_singleton_set(vc.ecsWorld, DeltaTime, {dT});

    ecs_progress(vc.ecsWorld, dT);

    if (world != NULL) {
        if (*ecs_singleton_get(vc.ecsWorld, ModelLastRenderedStep) < world->step) {
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
        ecs_singleton_set(vc.ecsWorld, ModelLastRenderedStep, {world->step});
    }

    // TODO: return elapsed time in ms
    return 0;
}

void bc_view_onModelStart(bc_ModelData *model) {
    ecs_singleton_set(vc.ecsWorld, ModelWorld, {model->world->ecsWorld});
    ecs_singleton_set(vc.ecsWorld, FocusPlane, {model->world->centralPlane});

    // TEST: ensure that import order hasn't caused mismatched component IDs
    ecs_entity_t modelPlaneId = ecs_lookup_fullpath(model->world->ecsWorld, "basaltic.components.planes.Plane");
    ecs_entity_t viewPlaneId = ecs_lookup_fullpath(vc.ecsWorld, "basaltic.components.planes.Plane"); //ecs_id(Plane);
    assert(modelPlaneId == viewPlaneId);

    bc_setCameraWrapLimits(vc.ecsWorld);

    ecs_run_pipeline(vc.ecsWorld, ModelChangedPipeline, 1.0f);
    // NOTE: model advances to step 1 before it becomes 'ready' (i.e. before onModelStart is called). Setting the last rendered step to 0 tells the view that model data is out of date on the next frame after model start
    ecs_singleton_set(vc.ecsWorld, ModelLastRenderedStep, {0});

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