#ifndef BASALTIC_MODEL_H_INCLUDED
#define BASALTIC_MODEL_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_worldState.h"

// TODO: track model tick count here?
typedef struct {
    bc_WorldState *world;
    bc_CommandBuffer processingBuffer;
    bool advanceSingleStep;
    bool autoStep;
} bc_ModelData;

typedef struct {
    u32 width;
    u32 height;
    char *seed;
} bc_ModelSetupSettings;

// NOTE: this is an awkward place to define this struct, because instances of it are owned by basaltic_super. However, because it requires structs defined in basaltic_model and is needed by bc_model_start, this is the easiest way to avoid circular includes
typedef struct {
    volatile bc_ProcessState *threadState;
    u32 interval;
    bc_CommandBuffer inputBuffer;
    bc_ModelSetupSettings *modelSettings;
    bool *isModelDataReady;
    bc_ModelData **modelDataRef; // modelData instance is created and destoryed by the model Module, this reference gives the supervisor a way to access modelData after starting the thread
} bc_ModelThreadInput;

void bc_model_argsToStartSettings(int argc, char *argv[], bc_ModelSetupSettings *destinationSettings);

/**
 * @brief Should NOT be called directly, instead use in a call to SDL_CreateThread
 *
 * @param in must be a pointer to bc_ModelThreadInput
 * @return thread exit status
 */
int bc_model_start(void *in);

void bc_model_cleanup(bc_ModelData *stoppedThreadModelData);

#endif // BASALTIC_MODEL_H_INCLUDED
