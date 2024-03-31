/*
 * This file is the interface from the core engine to the view, and remains the same when view modules are swapped out
 */
#ifndef BASALTIC_VIEW_H_INCLUDED
#define BASALTIC_VIEW_H_INCLUDED

#include <stdbool.h>
#include "basaltic_defs.h"
#include "basaltic_model.h"
#include "basaltic_window.h"

typedef struct {
    bc_SupervisorSignal signal;
} bc_SupervisorInterface;

void bc_view_setup(bc_WindowContext* wc);
void bc_view_teardown();

void bc_view_beginFrame(bc_WindowContext* wc);
void bc_view_endFrame(bc_WindowContext* wc);

void bc_view_onInputEvent(SDL_Event *e, bool useMouse, bool useKeyboard);
void bc_view_processInputState(bool useMouse, bool useKeyboard);

u32 bc_view_drawFrame(bc_SupervisorInterface* si, bc_WindowContext* wc);

void bc_view_setupEditor();
void bc_view_teardownEditor();
void bc_view_drawEditor(bc_SupervisorInterface* si);
void bc_view_drawGUI(bc_SupervisorInterface* si);

void bc_view_onModelStart(bc_ModelContext *mctx);
void bc_view_onModelStop(bc_ModelContext *mctx);

#endif // BASALTIC_VIEW_H_INCLUDED
