
#include <SDL2/SDL_mutex.h>
#include "htw_core.h"
#include "basaltic_commandBuffer.h"

typedef struct private_bc_CommandBuffer {
    ptrdiff_t *commandArenaOffsets;
    size_t maxCommandsInBuffer;
    size_t currentCommandsInBuffer;
    size_t bufferReadHead;

    size_t arenaSize;
    void *arena;
    void *arenaHead; // pointer to next available address in arena

    SDL_mutex *lock;
} private_bc_CommandBuffer;

bc_CommandBuffer bc_createCommandBuffer(u32 maxCommandsInBuffer, size_t arenaSize) {
    private_bc_CommandBuffer *newBuffer = malloc(sizeof(private_bc_CommandBuffer));
    newBuffer->commandArenaOffsets = calloc(maxCommandsInBuffer, sizeof(ptrdiff_t));
    newBuffer->maxCommandsInBuffer = maxCommandsInBuffer;
    newBuffer->currentCommandsInBuffer = 0;
    newBuffer->bufferReadHead = 0;

    newBuffer->arenaSize = arenaSize;
    newBuffer->arena = calloc(1, arenaSize);
    newBuffer->arenaHead = newBuffer->arena;

    newBuffer->lock = SDL_CreateMutex();
    return newBuffer;
}

void bc_destroyCommandBuffer(bc_CommandBuffer commandBuffer) {
    SDL_mutex *holdLock = commandBuffer->lock;
    SDL_LockMutex(holdLock);
    free(commandBuffer->commandArenaOffsets);
    free(commandBuffer->arena);
    free(commandBuffer);
    SDL_UnlockMutex(holdLock);
    SDL_DestroyMutex(holdLock);
}

bool bc_commandBufferIsEmpty(bc_CommandBuffer commandBuffer) {
    return commandBuffer->currentCommandsInBuffer == 0;
}

// NOTE: would be *very* useful to have something like Go's "defer" here for locks, to avoid mistakes causing hard to trace errors; what is the best way to handle this in c?
bool bc_pushCommandToBuffer(bc_CommandBuffer commandBuffer, const void* commandData, const size_t size) {
    SDL_LockMutex(commandBuffer->lock);
    if (commandBuffer->currentCommandsInBuffer == commandBuffer->maxCommandsInBuffer) {
        // command buffer is full
        SDL_UnlockMutex(commandBuffer->lock);
        return false;
    } else {
        if (commandBuffer->arenaHead + size > commandBuffer->arena + commandBuffer->arenaSize) {
            // Not enough space in arena
            SDL_UnlockMutex(commandBuffer->lock);
            return false;
        }
        void *copyDst = commandBuffer->arenaHead;
        ptrdiff_t arenaOffset = commandBuffer->arenaHead - commandBuffer->arena;
        commandBuffer->arenaHead += size;
        memcpy(copyDst, commandData, size);
        commandBuffer->commandArenaOffsets[commandBuffer->currentCommandsInBuffer++] = arenaOffset;
        SDL_UnlockMutex(commandBuffer->lock);
        return true;
    }
}

bool bc_transferCommandBuffer(bc_CommandBuffer dst, bc_CommandBuffer src) {
    SDL_LockMutex(src->lock);
    SDL_LockMutex(dst->lock);
    if (dst->currentCommandsInBuffer != 0 ||
        dst->maxCommandsInBuffer < src->currentCommandsInBuffer ||
        dst->arenaHead != dst->arena ||
        dst->arenaSize < src->arenaSize) {
        // destination isn't empty yet, or doesn't have enough space
        SDL_UnlockMutex(dst->lock);
        SDL_UnlockMutex(src->lock);
        return false;
    } else {
        size_t copySize = src->currentCommandsInBuffer * sizeof(src->commandArenaOffsets[0]);
        memcpy(dst->commandArenaOffsets, src->commandArenaOffsets, copySize);
        dst->currentCommandsInBuffer = src->currentCommandsInBuffer;
        src->currentCommandsInBuffer = 0;

        ptrdiff_t srcArenaUsedSize = src->arenaHead - src->arena;
        memcpy(dst->arena, src->arena, srcArenaUsedSize);
        src->arenaHead = src->arena;

        SDL_UnlockMutex(dst->lock);
        SDL_UnlockMutex(src->lock);
        return true;
    }
}

u32 bc_beginBufferProcessing(bc_CommandBuffer commandBuffer) {
    SDL_LockMutex(commandBuffer->lock);
    commandBuffer->bufferReadHead = 0;
    return commandBuffer->currentCommandsInBuffer;
}

void *bc_getNextCommand(bc_CommandBuffer commandBuffer){
    if (commandBuffer->bufferReadHead >= commandBuffer->maxCommandsInBuffer) {
        return NULL;
    }
    ptrdiff_t arenaOffset = commandBuffer->commandArenaOffsets[commandBuffer->bufferReadHead++];
    return commandBuffer->arena + arenaOffset;
}

void bc_endBufferProcessing(bc_CommandBuffer commandBuffer) {
    commandBuffer->currentCommandsInBuffer = 0;
    commandBuffer->arenaHead = commandBuffer->arena;
    commandBuffer->bufferReadHead = 0;
    SDL_UnlockMutex(commandBuffer->lock);
}
