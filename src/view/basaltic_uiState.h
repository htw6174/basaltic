#ifndef BASALTIC_UISTATE_H_INCLUDED
#define BASALTIC_UISTATE_H_INCLUDED

#include "htw_core.h"
#include "basaltic_defs.h"
#include "flecs.h"

typedef struct {
    bc_InterfaceMode interfaceMode;
    // world
    u32 hoveredChunkIndex;
    u32 hoveredCellIndex;
    u32 selectedChunkIndex;
    u32 selectedCellIndex;
    // menus
} bc_UiState;

bc_UiState *bc_createUiState();

#endif // BASALTIC_UISTATE_H_INCLUDED
