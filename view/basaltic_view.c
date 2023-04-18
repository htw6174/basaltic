#include "basaltic_view.h"
#include "basaltic_window.h"
#include "basaltic_editor.h"
#include "basaltic_interaction.h"
#include "basaltic_uiState.h"
#include "basaltic_components_view.h"
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
    ECS_IMPORT(vc.ecsWorld, BasalticComponentsView);
    ECS_IMPORT(vc.ecsWorld, BasalticSystemsView);

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
    sg_begin_default_pass(&pa, wc->width, wc->height);
}

void bc_view_endFrame(bc_WindowContext* wc) {
    sg_end_pass();
    sg_commit();
}

void bc_view_onInputEvent(bc_CommandBuffer inputBuffer, SDL_Event *e, bool useMouse, bool useKeyboard) {
    bc_processInputEvent(vc.ui, inputBuffer, e, useMouse, useKeyboard);
}

void bc_view_processInputState(bc_CommandBuffer inputBuffer, bool useMouse, bool useKeyboard) {
    bc_processInputState(vc.ecsWorld, inputBuffer, useMouse, useKeyboard);
}

u32 bc_view_drawFrame(bc_SupervisorInterface* si, bc_ModelData* model, bc_WindowContext* wc, bc_CommandBuffer inputBuffer) {
    ecs_singleton_set(vc.ecsWorld, WindowSize, {.x = wc->width, .y = wc->height});
    bc_WorldState *world = model == NULL ? NULL : model->world;
    // TODO
    //bc_updateRenderContextWithWorldParams(vc->rc, world);
    //bc_updateRenderContextWithUiState(vc.rc, wc, vc.ui);
    //bc_renderFrame(vc.rc, world);
    // TODO: if model is running, wait on lock semaphore before rendering (at least for model-reading pipelines)
    ecs_progress(vc.ecsWorld, 0.0f);
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
}

void bc_view_onModelStop() {
    ecs_singleton_remove(vc.ecsWorld, ModelWorld);
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
