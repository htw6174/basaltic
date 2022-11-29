#ifndef BASALTIC_EDITOR_H_INCLUDED
#define BASALTIC_EDITOR_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "htw_vulkan.h"
#include "basaltic_window.h"
#include "basaltic_uiState.h"
#include "basaltic_worldState.h"

typedef struct {
    bool isActive;
    htw_VkContext *vkContext;
    bool showDemoWindow;
} bc_EditorContext;

bc_EditorContext bc_initEditor(bool isActiveAtStart, htw_VkContext *vkContext);
void bc_destroyEditor(bc_EditorContext *editorContext);
void bc_resizeEditor(bc_EditorContext *editorContext);

bool bc_editorWantCaptureMouse(bc_EditorContext *editorContext);
bool bc_editorWantCaptureKeyboard(bc_EditorContext *editorContext);

void bc_handleEditorInputEvents(bc_EditorContext *editorContext, SDL_Event *e);
void bc_drawEditor(bc_EditorContext *editorContext, bc_GraphicsState *graphics, bc_UiState *ui, bc_WorldState *world);

#endif // BASALTIC_EDITOR_H_INCLUDED
