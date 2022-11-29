#ifndef BASALTIC_SUPER_H_INCLUDED
#define BASALTIC_SUPER_H_INCLUDED

#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_logic.h"
#include "basaltic_window.h"
#include "basaltic_interaction.h"
#include "basaltic_editor.h"

typedef struct {
    bc_StartupMode startupMode;
    char *dataDirectory;
    char *loadGamePath;
    char *newGameSeed;
} bc_StartupSettings;

typedef struct {
    u32 frameRateLimit; // Max rendering fps
    u32 tickRateLimit; // Max number of logic ticks per second (tps)
} bc_EngineSettings;

int bc_startEngine(bc_StartupSettings startSettings);

void bc_startNewGame(char *seed);
void bc_loadGame(char *path);
void bc_continueGame();

#endif // BASALTIC_SUPER_H_INCLUDED
