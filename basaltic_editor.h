#ifndef KINGDOM_EDITOR_H_INCLUDED
#define KINGDOM_EDITOR_H_INCLUDED

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
} bt_EditorContext;

bt_EditorContext bt_initEditor();
void bt_destroyEditor(bt_EditorContext *editorContext);
void bt_resizeEditor(bt_EditorContext *editorContext);

bool bt_editorWantCaptureMouse(bt_EditorContext *editorContext);
bool bt_editorWantCaptureKeyboard(bt_EditorContext *editorContext);

void bt_handleEditorInputEvents(bt_EditorContext *editorContext, SDL_Event *e);
void bt_drawEditor(bt_EditorContext *editorContext, bt_GraphicsState *graphics, bt_UiState *ui, bt_WorldState *world);

#endif // KINGDOM_EDITOR_H_INCLUDED
