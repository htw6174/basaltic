#include "basaltic_uiState.h"

bc_UiState* bc_createUiState() {
    bc_UiState *ui = malloc(sizeof(bc_UiState));
    *ui = (bc_UiState){
        .interfaceMode = BC_INTERFACE_MODE_SYSTEM_MENU,
        // world
        .hoveredCellIndex = 0,
        .selectedCellIndex = 0,
    };
    return ui;
}

