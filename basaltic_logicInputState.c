
#include <stdbool.h>
#include "basaltic_logicInputState.h"

bc_LogicInputState *bc_createLogicInputState() {
    bc_LogicInputState *newLogicInput = calloc(1, sizeof(bc_LogicInputState));
    newLogicInput->doAutoStep = false;
    newLogicInput->isEditPending = false;
    newLogicInput->isMovePending = false;
    return newLogicInput;
}

void bc_destroyLogicInputState(bc_LogicInputState *logicInput) {
    free(logicInput);
}
