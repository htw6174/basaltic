#ifndef BASALTIC_EDITOR_H_INCLUDED
#define BASALTIC_EDITOR_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_render.h"
#include "basaltic_uiState.h"

typedef struct {
    float *worldStepsPerSecond;

    bool isWorldGenerated;
    u32 worldChunkWidth;
    u32 worldChunkHeight;
    char newGameSeed[BC_MAX_SEED_LENGTH];
} bc_EditorContext;

void bc_setupEditor(void);
void bc_teardownEditor(void);
void bc_drawEditor(bc_SupervisorInterface* si, bc_ModelData* model, bc_CommandBuffer inputBuffer, bc_RenderContext *rc, bc_UiState *ui);


#endif // BASALTIC_EDITOR_H_INCLUDED
