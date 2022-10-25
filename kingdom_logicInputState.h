#ifndef KINGDOM_INPUTSTATE_H_INCLUDED
#define KINGDOM_INPUTSTATE_H_INCLUDED

#include <SDL_stdinc.h>
#include "htw_core.h"

typedef enum kd_MapEditType {
    KD_MAP_EDIT_SET,
    KD_MAP_EDIT_ADD,
} kd_MapEditType;

typedef struct {
    kd_MapEditType editType;
    u32 cellIndex;
    s32 value;
} kd_MapEditAction;

typedef struct {
    u32 maxPendingEdits;
    u32 head;
    u32 tail;
    kd_MapEditAction *pendingEdits;
} kd_PendingMapEditQueue;

typedef struct {
    u32 ticks;
    kd_MapEditAction currentEdit;
    _Bool isEditPending;
} kd_LogicInputState;

kd_LogicInputState *createLogicInputState();

#endif // KINGDOM_INPUTSTATE_H_INCLUDED
