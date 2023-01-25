#ifndef BASALTIC_EDITOR_BASE_H_INCLUDED
#define BASALTIC_EDITOR_BASE_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "htw_vulkan.h"
#include "basaltic_super.h"
#include "basaltic_window.h"

static const u32 bc_frameHistoryLength = 300;

typedef struct {
    bool isActive;
    htw_VkContext *vkContext;
    bool showDemoWindow;
    float maxFrameDuration;
    float maxStepsPerSecond;
    float *frameDurationHistory;
    float *tickDurationHistory;

    bool modelRestarting;
} bc_EditorEngineContext;

bc_EditorEngineContext bc_initEditor(bool isActiveAtStart, htw_VkContext *vkContext);
void bc_destroyEditor(bc_EditorEngineContext *eec);
void bc_resizeEditor(bc_EditorEngineContext *eec);

bool bc_editorWantCaptureMouse(bc_EditorEngineContext *eec);
bool bc_editorWantCaptureKeyboard(bc_EditorEngineContext *eec);

void bc_handleEditorInputEvents(bc_EditorEngineContext *eec, SDL_Event *e);
void bc_beginEditor();
void bc_endEditor(bc_EditorEngineContext *eec);
void bc_drawEditor(bc_EditorEngineContext *eec, bc_WindowContext *wc, bc_SuperInfo *superInfo);

#endif // BASALTIC_EDITOR_BASE_H_INCLUDED
