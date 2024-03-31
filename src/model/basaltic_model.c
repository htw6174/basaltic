#include <SDL2/SDL.h>
#include "basaltic_model.h"
#include "basaltic_components.h"
#include "basaltic_systems.h"

ecs_world_t *model_createWorld(int argc, char *argv[]) {
#ifdef FLECS_SANITIZE
    printf("Initializing flecs in sanitizing mode. Expect a significant slowdown.\n");
#endif
    ecs_world_t *world = ecs_init();
    // TODO: configuration for model worker threads
    //ecs_set_threads(newWorld->ecsWorld, 4);
    //ecs_set_stage_count(newWorld->ecsWorld, 2);
    ECS_IMPORT(world, Bc);
    ECS_IMPORT(world, BcSystems);

    ecs_singleton_set(world, Args, {argc, argv});

    // FIXME: for now, need to progress world once before it can be considered "ready"
    ecs_progress(world, 1.0);

    // TODO: script setup as part of a "game" module?
    //ecs_plecs_from_file(world, "model/plecs/startup/startup_test.flecs");
    //ecs_set_pair(world, 0, ResourceFile, FlecsScriptSource, {.path = "model/plecs/startup/startup_test.flecs"});
    ecs_set_pair(world, 0, ResourceFile, FlecsScriptSource, {.path = "model/plecs/test.flecs"});

    return world;
}

void model_progressWorld(bc_ModelContext *mctx) {
    Uint64 startTime = SDL_GetTicks64();
    ecs_progress(mctx->world, mctx->deltaTime);
    // get tick duration for stats
    Uint64 endTime = SDL_GetTicks64();
    Uint64 duration = endTime - startTime;
    // TODO: where to store model performance stats?
    mctx->step++;
}

void model_destroyWorld(ecs_world_t *world) {
    ecs_fini(world);
}

int bc_model_run(void* in) {
    // Extract input data
    bc_ModelThreadInput *threadInput = (bc_ModelThreadInput*)in;
    bc_ModelContext *modelContext = threadInput->modelContext;

    SDL_LockMutex(modelContext->mutex);

    modelContext->world = model_createWorld(threadInput->argc, threadInput->argv);
    *threadInput->isModelDataReady = true;

    while (!modelContext->shouldStopModel) {
        // wait to be signaled
        SDL_CondWait(modelContext->cond, modelContext->mutex);

        for (int i = 0; i < modelContext->runForSteps; i++) {
            model_progressWorld(modelContext);
        }
    }
    *threadInput->isModelDataReady = false;
    model_destroyWorld(modelContext->world);

    SDL_UnlockMutex(modelContext->mutex);

    return 0;
}
