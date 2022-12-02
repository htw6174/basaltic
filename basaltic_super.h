#ifndef BASALTIC_SUPER_H_INCLUDED
#define BASALTIC_SUPER_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_defs.h"

static const u32 bc_frameHistoryLength = 300;

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
    // TODO: there is already a frame counter defined in graphicsState; move here, redefine here, or leave as-is?
    u64 *frameDurations;
    u64 *tickDurations;
    u64 *worldStepHistory;
} bc_SuperInfo;

int bc_startEngine(bc_StartupSettings startSettings);

void bc_startNewGame(char *seed);
void bc_loadGame(char *path);
void bc_continueGame();
void bc_requestGameStop();
void bc_requestProcessStop();
bool bc_isGameRunning();

#endif // BASALTIC_SUPER_H_INCLUDED
