#include "basaltic_logicInputState.h"

bt_LogicInputState *createLogicInputState() {
    bt_LogicInputState *newState = malloc(sizeof(bt_LogicInputState));
    return newState;
}
