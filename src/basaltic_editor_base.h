#ifndef BASALTIC_EDITOR_BASE_H_INCLUDED
#define BASALTIC_EDITOR_BASE_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_super.h"
#include "basaltic_window.h"

typedef struct {
    bool isActive;
    bool showDemoWindow;
    float *frameTimes;

    bool modelRestarting;
} bc_EditorEngineContext;

bc_EditorEngineContext bc_initEditor(bool isActiveAtStart, bc_WindowContext *wc);
void bc_destroyEditor(bc_EditorEngineContext *eec);
void bc_resizeEditor(bc_EditorEngineContext *eec);

bool bc_editorWantCaptureMouse(bc_EditorEngineContext *eec);
bool bc_editorWantCaptureKeyboard(bc_EditorEngineContext *eec);

void bc_handleEditorInputEvents(bc_EditorEngineContext *eec, SDL_Event *e);
void bc_beginEditor();
void bc_endEditor();
void bc_drawBaseEditor(bc_EditorEngineContext *eec, bc_WindowContext *wc, bc_SuperInfo *superInfo, bc_EngineSettings *engineSettings);

#endif // BASALTIC_EDITOR_BASE_H_INCLUDED
