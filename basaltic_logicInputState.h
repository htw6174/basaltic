#ifndef KINGDOM_INPUTSTATE_H_INCLUDED
#define KINGDOM_INPUTSTATE_H_INCLUDED

#include "htw_core.h"

typedef enum bt_MapEditType {
    KD_MAP_EDIT_SET,
    KD_MAP_EDIT_ADD,
} bt_MapEditType;

typedef struct {
    bt_MapEditType editType;
    u32 chunkIndex;
    u32 cellIndex;
    s32 value;
} bt_MapEditAction;

typedef struct {
    u32 characterId;
    u32 chunkIndex;
    u32 cellIndex;
} bt_CharacterMoveAction;

typedef struct {
    u32 maxPendingEdits;
    u32 head;
    u32 tail;
    bt_MapEditAction *pendingEdits;
} bt_PendingMapEditQueue;

typedef struct {
    u32 ticks;
    bt_MapEditAction currentEdit;
    _Bool isEditPending;
    bt_CharacterMoveAction currentMove;
    _Bool isMovePending;
} bt_LogicInputState;

bt_LogicInputState *createLogicInputState();

#endif // KINGDOM_INPUTSTATE_H_INCLUDED
