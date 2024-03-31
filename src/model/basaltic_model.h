#ifndef BASALTIC_MODEL_H_INCLUDED
#define BASALTIC_MODEL_H_INCLUDED

#include <stdbool.h>
#include <SDL2/SDL_mutex.h>
#include "basaltic_defs.h"
#include "flecs.h"

// NOTE: shared resource. When receiving a pointer to bc_ModelData, MUST lock mutex before use.
typedef struct {
    SDL_mutex *mutex;
    SDL_cond *cond;
    // Should aquire lock before using other members, but in theory won't break anything to use these out of a lock
    bool shouldStopModel; // set to true then signal cond to stop model thread
    int runForSteps; // set then signal cond to run model for the specified number of steps
    uint64_t step; // current model step
    // Must aquire lock to use members from here down
    ecs_world_t *world;
    float deltaTime;
} bc_ModelContext;

// NOTE: this is an awkward place to define this struct, because instances of it are owned by basaltic_super. However, because it requires structs defined in basaltic_model and is needed by bc_model_start, this is the easiest way to avoid circular includes
typedef struct {
    int argc;
    char **argv;
    bool *isModelDataReady;
    bc_ModelContext *modelContext; // Shared resource, always lock model.mutex before using
} bc_ModelThreadInput;

/**
 * @brief Entry point for model thread, use with SDL_CreateThread
 *
 * @param in must be a pointer to bc_ModelThreadInput
 * @return thread exit status
 */
int bc_model_run(void *in);

#endif // BASALTIC_MODEL_H_INCLUDED
