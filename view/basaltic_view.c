#include "basaltic_view.h"
#include "basaltic_window.h"
#include "basaltic_render.h"
#include "basaltic_editor.h"
#include "basaltic_interaction.h"
#include "basaltic_uiState.h"
#include "flecs.h"

#define SOKOL_IMPL
#define SOKOL_GLCORE33
#define SOKOL_NO_DEPRECATED
#ifdef DEBUG
#define SOKOL_DEBUG
#endif
#include "sokol/sokol_log.h"
#include "sokol/sokol_gfx.h"

// TODO: add 'guard' value to ensure valid state before being used? This is only used in my core engine, so may be unnecessary
typedef struct {
    bc_RenderContext *rc;
    bc_UiState *ui;
} bc_ViewContext;

static bc_ViewContext vc;

void bc_view_setup(bc_WindowContext* wc) {
    sg_desc sgd = {
        .logger.func = slog_func,
    };
    sg_setup(&sgd);
    assert(sg_isvalid());

    vc.rc = bc_createRenderContext(wc);
    vc.ui = bc_createUiState();
}

void bc_view_teardown() {
    sg_shutdown();
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
    bc_processInputState(vc.ui, inputBuffer, useMouse, useKeyboard);
}

u32 bc_view_drawFrame(bc_SupervisorInterface* si, bc_ModelData* model, bc_WindowContext* wc, bc_CommandBuffer inputBuffer) {
    bc_WorldState *world = model == NULL ? NULL : model->world;
    // TODO
    //bc_updateRenderContextWithWorldParams(vc->rc, world);
    bc_updateRenderContextWithUiState(vc.rc, wc, vc.ui);
    bc_renderFrame(vc.rc, world);
    // TODO: return elapsed time in ms
    return 0;
}

void bc_view_onModelStart(bc_ModelData *model) {
    bc_setModelEcsWorld(vc.ui, model->world->ecsWorld);
    bc_setFocusedTerrain(vc.ui, model->world->centralPlane);
    //bc_updateRenderContextWithWorldParams(vc->rc, model->world);
}

void bc_view_onModelStop() {
    // TODO: reset uiState that references world data
    bc_setModelEcsWorld(vc.ui, NULL);
}

void bc_view_setupEditor() {
    // TODO: can handle renderer-specific imgui setup here
    bc_setupEditor();
}

void bc_view_teardownEditor() {
    bc_teardownEditor();
}

void bc_view_drawEditor(bc_SupervisorInterface* si, bc_ModelData* model, bc_CommandBuffer inputBuffer) {
    bc_drawEditor(si, model, inputBuffer, vc.rc, vc.ui);
}
