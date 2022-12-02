/*
 * Note on how bc_CommandQueue should be used:
 * A pair of bc_CommandQueues are used to send commands related to world state changes from the main thread to the logic thread
 * Both queues are to be locked before any reads or writes, and unlocked immediately after
 * The main thread's 'input' command queue has commands added by the main thread, and is periodically emptied by the logic thread
 * The logic thread's 'processing' command queue is processed from start to finish (index 0..itemsInQueue) every logic tick, and only after the processing queue is cleared the logic thread copies all items from the input queue into the processing queue (clearing the input queue)
 * TODO: is queue a misleading name here? Behavior may be closer to command buffers as used in graphics APIs. Array is still processed in first in/first out order, but without complete push/pop support
 *
 */

#ifndef BASALTIC_COMMANDQUEUE_H_INCLUDED
#define BASALTIC_COMMANDQUEUE_H_INCLUDED

#include <stdbool.h>
#include <SDL2/SDL_mutex.h>
#include "htw_core.h"

typedef enum bc_CommandType {
    BC_COMMAND_TYPE_CHARACTER_MOVE,
    BC_COMMAND_TYPE_TERRAIN_EDIT,
    BC_COMMAND_TYPE_STEP_ADVANCE,
    BC_COMMAND_TYPE_STEP_PLAY,
    BC_COMMAND_TYPE_STEP_PAUSE
} bc_CommandType;

typedef struct {
    bc_CommandType commandType;
} bc_WorldInputCommand;

// Make commandQueue struct opaque to avoid access outside of lock
typedef struct private_bc_CommandQueue* bc_CommandQueue;

bc_CommandQueue bc_createCommandQueue(u32 maxQueueSize);
void bc_destroyCommandQueue(bc_CommandQueue commandQueue);

bool bc_commandQueueIsEmpty(bc_CommandQueue commandQueue);

/**
 * @brief Attempt to add command to the end of commandQueue. Will wait for commandQueue to be unlocked, and lock the queue while adding command
 *
 * @param commandQueue p_commandQueue:...
 * @return true on success, false if the queue is full
 */
bool bc_pushCommandToQueue(bc_CommandQueue commandQueue, bc_WorldInputCommand command);

/**
 * @brief Attempt to copy all commands from src to dst, then empty src. Will wait for locks on both queues to be released, and lock both queues while copying
 *
 * @param dst p_dst:...
 * @param src p_src:...
 * @return true on success, false if dst contains any items when called or if dst is smaller than src
 */
bool bc_transferCommandQueue(bc_CommandQueue dst, bc_CommandQueue src);

/**
 * @brief Locks commandQueue, assigns *firstCommand to the start of the queue array, and returns the number of commands in the queue
 *
 * @param commandQueue p_commandQueue:...
 * @param firstCommand value will be assigned to the first element of commandArray
 * @return number of commands in the queue
 */
u32 bc_beginQueueProcessing(bc_CommandQueue commandQueue, bc_WorldInputCommand *firstCommand[]);

/**
 * @brief Empties and unlocks commandQueue
 *
 * @param commandQueue p_commandQueue:...
 */
void bc_endQueueProcessing(bc_CommandQueue commandQueue);

#endif // BASALTIC_COMMANDQUEUE_H_INCLUDED
