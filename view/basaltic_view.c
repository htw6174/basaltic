#include "basaltic_view.h"
#include "basaltic_window.h"
#include "basaltic_render.h"
#include "basaltic_interaction.h"

bc_ViewContext* bc_view_setup(bc_WindowContext* wc) {
    bc_ViewContext *vc = malloc(sizeof(bc_ViewContext));
    //vc->graphics = bc_createGraphicsState();
    vc->rc = bc_createRenderContext(wc);

    vc->ui = bc_createUiState();

    return vc;
}

void bc_view_teardown(bc_ViewContext* vc) {
    // TODO
}

void bc_view_onInputEvent(bc_ViewContext* vc, bc_CommandBuffer inputBuffer, SDL_Event *e, bool useMouse, bool useKeyboard) {
    bc_processInputEvent(vc->ui, inputBuffer, e, useMouse, useKeyboard);
}

void bc_view_processInputState(bc_ViewContext* vc, bc_CommandBuffer inputBuffer, bool useMouse, bool useKeyboard) {
    bc_processInputState(vc->ui, inputBuffer, useMouse, useKeyboard);
}

u32 bc_view_drawFrame(bc_SupervisorInterface* si, bc_ViewContext* vc, bc_ModelData* model, bc_WindowContext* wc, bc_CommandBuffer inputBuffer) {
    bc_WorldState *world = model == NULL ? NULL : model->world;
    // TODO
    //bc_updateRenderContextWithWorldParams(vc->rc, world);
    bc_updateRenderContextWithUiState(vc->rc, wc, vc->ui);
    bc_renderFrame(vc->rc, world);
    // TODO: return elapsed time in ms
    return 0;
}

void bc_view_onModelStart(bc_ViewContext *vc, bc_ModelData *model) {
    bc_WorldState *world = model->world;
    bc_SetCameraWrapLimits(vc->ui, world->surfaceMap->mapWidth, world->surfaceMap->mapHeight);
    bc_updateRenderContextWithWorldParams(vc->rc, model->world);
}

void bc_view_onModelStop(bc_ViewContext *vc) {
    // TODO: reset uiState that references world data
}
