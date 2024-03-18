#ifndef BASALTIC_EDITOR_H_INCLUDED
#define BASALTIC_EDITOR_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_uiState.h"
#include "flecs.h"

void bc_setupEditor(void);
void bc_teardownEditor(void);
void bc_drawEditor(bc_SupervisorInterface* si, bc_ModelData* model, ecs_world_t *viewWorld, bc_UiState *ui);
void bc_drawGUI(bc_SupervisorInterface* si, bc_ModelData* model, ecs_world_t *viewWorld);


void bc_editorOnModelStart(void);
void bc_editorOnModelStop(void);


#endif // BASALTIC_EDITOR_H_INCLUDED
