
#include <stdbool.h>
#include "basaltic_logicInputState.h"

bc_LogicInputState *bc_createLogicInputState() {
    bc_LogicInputState *newState = calloc(1, sizeof(bc_LogicInputState));
    newState->isEditPending = false;
    newState->isMovePending = false;
    return newState;
}

void bc_destroyLogicInputState(bc_LogicInputState *logicInput) {
    free(logicInput);
}
