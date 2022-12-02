
#include <SDL2/SDL_mutex.h>
#include "htw_core.h"
#include "basaltic_commandQueue.h"

typedef struct private_bc_CommandQueue {
    bc_WorldInputCommand *commandsArray;
    u32 maxQueueSize;
    u32 itemsInQueue;
    SDL_mutex *lock;
} private_bc_CommandQueue;

bc_CommandQueue bc_createCommandQueue(u32 maxQueueSize) {
    private_bc_CommandQueue *newQueue = malloc(sizeof(private_bc_CommandQueue));
    newQueue->commandsArray = calloc(maxQueueSize, sizeof(bc_WorldInputCommand));
    newQueue->maxQueueSize = maxQueueSize;
    newQueue->itemsInQueue = 0;
    newQueue->lock = SDL_CreateMutex();
    return newQueue;
}

void bc_destroyCommandQueue(bc_CommandQueue commandQueue) {
    SDL_mutex *holdLock = commandQueue->lock;
    SDL_LockMutex(holdLock);
    free(commandQueue->commandsArray);
    free(commandQueue);
    SDL_UnlockMutex(holdLock);
    SDL_DestroyMutex(holdLock);
}

bool bc_pushCommandToQueue(bc_CommandQueue commandQueue, bc_WorldInputCommand command) {
    SDL_LockMutex(commandQueue->lock);
    if (commandQueue->itemsInQueue == commandQueue->maxQueueSize) {
        SDL_UnlockMutex(commandQueue->lock);
        return false;
    } else {
        commandQueue->commandsArray[commandQueue->itemsInQueue++] = command;
        SDL_UnlockMutex(commandQueue->lock);
        return true;
    }
}

bool bc_transferCommandQueue(bc_CommandQueue dst, bc_CommandQueue src) {
    SDL_LockMutex(src->lock);
    SDL_LockMutex(dst->lock);
    if (dst->itemsInQueue != 0 || dst->maxQueueSize < src->maxQueueSize) {
        SDL_UnlockMutex(dst->lock);
        SDL_UnlockMutex(src->lock);
        return false;
    } else {
        size_t copySize = src->itemsInQueue * sizeof(src->commandsArray[0]);
        memcpy(dst->commandsArray, src->commandsArray, copySize);
        dst->itemsInQueue = src->itemsInQueue;
        src->itemsInQueue = 0;

        SDL_UnlockMutex(dst->lock);
        SDL_UnlockMutex(src->lock);
        return true;
    }
}

u32 bc_beginQueueProcessing(bc_CommandQueue commandQueue, bc_WorldInputCommand *firstCommand[]) {
    SDL_LockMutex(commandQueue->lock);
    *firstCommand = commandQueue->commandsArray;
    return commandQueue->itemsInQueue;
}

void bc_endQueueProcessing(bc_CommandQueue commandQueue) {
    commandQueue->itemsInQueue = 0;
    SDL_UnlockMutex(commandQueue->lock);
}
