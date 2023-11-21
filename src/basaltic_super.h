#ifndef BASALTIC_SUPER_H_INCLUDED
#define BASALTIC_SUPER_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_model.h"

#define BC_FRAME_HISTORY_LENGTH 300

typedef struct {
    bc_StartupMode startupMode;
    bool enableEditor;
    char *dataDirectory;
    char *loadModelPath;
    size_t startModelArgCount;
    char **startModelArgs;
} bc_StartupSettings;

typedef struct {
    u32 frameRateLimit; // Max rendering fps
    u32 tickRateLimit; // Max number of logic ticks per second (tps)
} bc_EngineSettings;

typedef struct {
    u64 frameCPUTimes[BC_FRAME_HISTORY_LENGTH]; // time from start to end of main loop SDL_GetPerformanceCounter calls
    u64 frameTotalTimes[BC_FRAME_HISTORY_LENGTH]; // time from start of one main loop iteration to the next
} bc_SuperInfo;

int bc_startEngine(bc_StartupSettings startSettings);

#endif // BASALTIC_SUPER_H_INCLUDED
