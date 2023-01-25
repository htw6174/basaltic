#ifndef BASALTIC_VIEW_H_INCLUDED
#define BASALTIC_VIEW_H_INCLUDED

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "basaltic_defs.h"
#include "basaltic_model.h"
#include "basaltic_window.h"
#include "basaltic_render.h"
#include "basaltic_worldState.h"
#include "basaltic_uiState.h"

typedef struct {
    bc_RenderContext *rc;
    bc_UiState *ui;
} bc_ViewContext;

typedef struct {
    float *worldStepsPerSecond;

    bool isWorldGenerated;
    u32 worldChunkWidth;
    u32 worldChunkHeight;
    char newGameSeed[BC_MAX_SEED_LENGTH];
} bc_EditorContext;

typedef struct {
    bc_SupervisorSignal signal;
    bc_ModelSetupSettings modelSettings;
} bc_SupervisorInterface;

bc_ViewContext* bc_view_setup(bc_WindowContext* wc);
void bc_view_teardown(bc_ViewContext* vc);
void bc_view_onInputEvent(bc_ViewContext* vc, bc_CommandBuffer inputBuffer, SDL_Event *e, bool useMouse, bool useKeyboard);
void bc_view_processInputState(bc_ViewContext* vc, bc_CommandBuffer inputBuffer, bool useMouse, bool useKeyboard);
u32 bc_view_drawFrame(bc_SupervisorInterface* si, bc_ViewContext* vc, bc_ModelData* model, bc_WindowContext* wc, bc_CommandBuffer inputBuffer);

// defined in basaltic_editor.h
bc_EditorContext* bc_view_setupEditor();
void bc_view_teardownEditor(bc_EditorContext* ec);
void bc_view_drawEditor(bc_SupervisorInterface* si, bc_EditorContext* ec, bc_ViewContext* vc, bc_ModelData* model, bc_CommandBuffer inputBuffer);

void bc_view_onModelStart(bc_ViewContext *vc, bc_ModelData *model);
void bc_view_onModelStop(bc_ViewContext *vc);

#endif // BASALTIC_VIEW_H_INCLUDED
