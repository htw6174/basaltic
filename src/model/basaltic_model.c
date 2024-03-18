#include <stdbool.h>
#include <SDL2/SDL.h>
#include "basaltic_model.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_logic.h"

void bc_model_argsToStartSettings(int argc, char *argv[], bc_ModelSetupSettings *destinationSettings) {
    // assign defaults
    destinationSettings->seed = "6174";
    destinationSettings->width = 3;
    destinationSettings->height = 3;

    // TODO: have this change settings according to name provided in arg; for now will use no names and expect a specific order
    for (int i = 0; i < argc; i++) {
        switch (i) {
            case 0:
                destinationSettings->seed = calloc(BC_MAX_SEED_LENGTH, sizeof(char));
                strcpy(destinationSettings->seed, argv[i]);
                break;
            case 1:
                destinationSettings->width = htw_strToInt(argv[i]);
                break;
            case 2:
                destinationSettings->height = htw_strToInt(argv[i]);
                break;
            default:
                break;
        }
    }
}

int bc_model_start(void* in) {
    // Extract input data
    bc_ModelThreadInput *threadInput = (bc_ModelThreadInput*)in;

    bc_ModelSetupSettings *modelSettings = threadInput->modelSettings;
    bc_ModelData *modelData = threadInput->modelData;

    SDL_LockMutex(modelData->mutex);

    modelData->world = bc_createWorldState(modelSettings->width, modelSettings->height, modelSettings->seed);
    bc_initializeWorldState(modelData->world);

    *threadInput->isModelDataReady = true;

    while (!modelData->shouldStopModel) {
        // wait to be signaled
        if (SDL_CondWait(modelData->cond, modelData->mutex)) {
            printf("wat");
        }

        Uint64 startTime = SDL_GetTicks64();

        for (int i = 0; i < modelData->runForSteps; i++) {
            bc_doLogicTick(modelData->world);
        }

        // get tick duration for stats
        Uint64 endTime = SDL_GetTicks64();
        Uint64 duration = endTime - startTime;
        // TODO: where to store model performance stats?

    }
    *threadInput->isModelDataReady = false;
    bc_destroyWorldState(modelData->world);

    SDL_UnlockMutex(modelData->mutex);

    return 0;
}
