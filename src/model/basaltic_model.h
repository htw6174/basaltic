#ifndef BASALTIC_MODEL_H_INCLUDED
#define BASALTIC_MODEL_H_INCLUDED

#include <stdbool.h>
#include <SDL2/SDL_mutex.h>
#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_worldState.h"

// NOTE: shared resource. When receiving a pointer to bc_ModelData, MUST lock mutex before use.
typedef struct {
    bc_WorldState *world; // Once lock is aquired, pass WorldState further down
    SDL_mutex *mutex;
    SDL_cond *cond;
    int runForSteps; // set this, then signal cond to run model for the specified number of steps
    bool shouldStopModel;
} bc_ModelData;

typedef struct {
    u32 width;
    u32 height;
    char *seed;
} bc_ModelSetupSettings;

// NOTE: this is an awkward place to define this struct, because instances of it are owned by basaltic_super. However, because it requires structs defined in basaltic_model and is needed by bc_model_start, this is the easiest way to avoid circular includes
typedef struct {
    bc_ModelSetupSettings *modelSettings;
    bool *isModelDataReady;
    bc_ModelData *modelData; // Shared resource, always lock model.mutex before using
} bc_ModelThreadInput;

void bc_model_argsToStartSettings(int argc, char *argv[], bc_ModelSetupSettings *destinationSettings);

/**
 * @brief Should NOT be called directly, instead use in a call to SDL_CreateThread
 *
 * @param in must be a pointer to bc_ModelThreadInput
 * @return thread exit status
 */
int bc_model_start(void *in);

static bool bc_model_isRunning(bc_ModelData *model) {
    // TODO: either need to rework this based on new threading model, or ensure any model access is inside lock
    return true;
}

#endif // BASALTIC_MODEL_H_INCLUDED
