#include "basaltic_logicInputState.h"

bc_LogicInputState *createLogicInputState() {
    bc_LogicInputState *newState = malloc(sizeof(bc_LogicInputState));
    return newState;
}
