#include <stdbool.h>
#include <SDL2/SDL.h>
#include "basaltic_model.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_logic.h"

int bc_model_start(void* in) {
    // Extract input data
    bc_ModelThreadInput *threadInput = (bc_ModelThreadInput*)in;

    volatile bc_ProcessState *threadState = threadInput->threadState;
    Uint32 interval = threadInput->interval;
    bc_CommandBuffer inputBuffer = threadInput->inputBuffer;
    bc_ModelSetupSettings *modelSettings = threadInput->modelSettings;
    bc_ModelData **modelDataRef = threadInput->modelDataRef;

    bc_ModelData *modelData = calloc(1, sizeof(bc_ModelData));
    *modelData = (bc_ModelData){
        .world = bc_createWorldState(modelSettings->width, modelSettings->height, modelSettings->seed),
        .processingBuffer = bc_createCommandBuffer(bc_commandBufferMaxCommandCount, bc_commandBufferMaxArenaSize),
        .advanceSingleStep = false,
        .autoStep = false,
    };
    *modelDataRef = modelData;

    bc_initializeWorldState(modelData->world);
    *threadInput->isModelDataReady = true;

    while (*threadState == BC_PROCESS_STATE_RUNNING) {
        Uint64 startTime = SDL_GetTicks64(); // TODO: use something other than SDL for timing, so the dependency can be removed

        bc_doLogicTick(modelData, inputBuffer);

        // delay until end of frame
        Uint64 endTime = SDL_GetTicks64();
        Uint64 duration = endTime - startTime;
        if (duration < interval) {
            SDL_Delay(interval - duration);
        }
    }

    *threadInput->isModelDataReady = false;

    return 0;
}

void bc_model_cleanup(bc_ModelData *stoppedThreadModelData) {
    bc_destroyWorldState(stoppedThreadModelData->world);
    bc_destroyCommandBuffer(stoppedThreadModelData->processingBuffer);
    free(stoppedThreadModelData);
}
