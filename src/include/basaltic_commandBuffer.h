/*
 * Note on how bc_CommandBuffer should be used:
 * A pair of bc_CommandBuffers are used to send commands related to world state changes from the main thread to the logic thread
 * Both queues are to be locked before any reads or writes, and unlocked immediately after
 * The main thread's 'input' command queue has commands added by the main thread, and is periodically emptied by the logic thread
 * The logic thread's 'processing' command queue is processed from start to finish (index 0..itemsInBuffer) every logic tick, and only after the processing queue is cleared the logic thread copies all items from the input queue into the processing queue (clearing the input queue)
 *
 */

#ifndef BASALTIC_COMMANDBUFFER_H_INCLUDED
#define BASALTIC_COMMANDBUFFER_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"

static const size_t bc_commandBufferMaxCommandCount = 1000;
static const size_t bc_commandBufferMaxArenaSize = 1 << 24; // = 4 MB

// Make commandBuffer struct opaque to avoid access outside of lock
typedef struct private_bc_CommandBuffer* bc_CommandBuffer;

bc_CommandBuffer bc_createCommandBuffer(u32 maxCommandsInBuffer, size_t arenaSize);
void bc_destroyCommandBuffer(bc_CommandBuffer commandBuffer);

bool bc_commandBufferIsEmpty(bc_CommandBuffer commandBuffer);

/**
 * @brief Attempt to add command to the end of commandBuffer. Will wait for commandBuffer to be unlocked, and lock the buffer while adding command
 *
 * @param commandData
 * @param size
 * @return true on success, false if command data size > available arena memory or if the command buffer is full
 */
bool bc_pushCommandToBuffer(bc_CommandBuffer commandBuffer, const void* commandData, const size_t size);

/**
 * @brief Attempt to copy all commands from src to dst, then empty src. Will wait for locks on both buffers to be released, and lock both buffers while copying
 *
 * @param dst p_dst:...
 * @param src p_src:...
 * @return true on success, false if dst contains any items when called or if dst is smaller than src
 */
bool bc_transferCommandBuffer(bc_CommandBuffer dst, bc_CommandBuffer src);

u32 bc_beginBufferProcessing(bc_CommandBuffer commandBuffer);

void *bc_getNextCommand(bc_CommandBuffer commandBuffer);

/**
 * @brief Empties and unlocks commandBuffer
 *
 * @param commandBuffer p_commandBuffer:...
 */
void bc_endBufferProcessing(bc_CommandBuffer commandBuffer);

#endif // BASALTIC_COMMANDBUFFER_H_INCLUDED
