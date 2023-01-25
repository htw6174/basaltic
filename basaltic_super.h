#ifndef BASALTIC_SUPER_H_INCLUDED
#define BASALTIC_SUPER_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_model.h"

typedef struct {
    bc_StartupMode startupMode;
    bool enableEditor;
    char *dataDirectory;
    char *loadGamePath;
    char *newGameSeed;
} bc_StartupSettings;

typedef struct {
    u32 frameRateLimit; // Max rendering fps
    u32 tickRateLimit; // Max number of logic ticks per second (tps)
} bc_EngineSettings;

typedef struct {
    // TODO: move frame time tracking to bc_WindowContext?
    u64 *frameDurations;
    u64 *tickDurations;
    u64 *worldStepHistory;
} bc_SuperInfo;

int bc_startEngine(bc_StartupSettings startSettings);

#endif // BASALTIC_SUPER_H_INCLUDED
