#ifndef KINGDOM_INTERACTION_H_INCLUDED
#define KINGDOM_INTERACTION_H_INCLUDED

#include "kingdom_defs.h"
#include "kingdom_uiState.h"
#include "kingdom_logicInputState.h"

int kd_handleInputs(kd_UiState *ui, kd_LogicInputState *logicInput, KD_APPSTATE *volatile appState);

#endif // KINGDOM_INTERACTION_H_INCLUDED
