#ifndef BASALTIC_INPUTSTATE_H_INCLUDED
#define BASALTIC_INPUTSTATE_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_characters.h"

typedef enum bc_MapEditType {
    BC_MAP_EDIT_SET,
    BC_MAP_EDIT_ADD,
} bc_MapEditType;

typedef struct {
    bc_MapEditType editType;
    u32 chunkIndex;
    u32 cellIndex;
    s32 value;
} bc_MapEditAction;

typedef struct {
    bc_Character *character;
    u32 chunkIndex;
    u32 cellIndex;
} bc_CharacterMoveAction;

typedef struct {
    u32 maxPendingEdits;
    u32 head;
    u32 tail;
    bc_MapEditAction *pendingEdits;
} bc_PendingMapEditQueue;

typedef struct {
    u32 ticks;
    bc_MapEditAction currentEdit;
    bool isEditPending;
    bc_CharacterMoveAction currentMove;
    bool isMovePending;
} bc_LogicInputState;

bc_LogicInputState *bc_createLogicInputState();

#endif // BASALTIC_INPUTSTATE_H_INCLUDED
