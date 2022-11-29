#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>
#include "basaltic_defs.h"
#include "basaltic_super.h"
#include "htw_core.h"
#include "htw_vulkan.h"

bc_StartupSettings parseArgs(int argc, char *argv[]);

bc_StartupSettings parseArgs(int argc, char *argv[]) {
    // default settings
    bc_StartupSettings settings = {
        .startupMode = BC_STARTUP_MODE_MAINMENU,
        .dataDirectory = "..",
        .loadGamePath = "",
        .newGameSeed = "6174",
    };

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] == '-') {
            switch (arg[1]) {
                case 'n':
                    settings.startupMode = BC_STARTUP_MODE_NEWGAME;
                    settings.newGameSeed = argv[i + 1];
                    i++;
                    printf("Starting new game with seed '%s'\n", settings.newGameSeed);
                    break;
                case 'l':
                    settings.startupMode = BC_STARTUP_MODE_LOADGAME;
                    settings.loadGamePath = argv[i + 1];
                    i++;
                    printf("Loading save '%s'\n", settings.loadGamePath);
                    break;
                case 'c':
                    settings.startupMode = BC_STARTUP_MODE_CONTINUEGAME;
                    settings.loadGamePath = ""; // TODO: determine path of most recent save game here or somewhere else?
                    printf("Loading most recent save\n");
                    break;
                case 'd':
                    settings.dataDirectory = argv[i + 1];
                    i++;
                    break;
                default:
                    fprintf(stderr, "ERROR: unrecognized option '%s'\n", arg);
                    exit(1);
            }
        } else {
            fprintf(stderr, "ERROR: no option specified for arg '%s'\n", arg);
            exit(1);
        }
    }

    return settings;
}

int main(int argc, char *argv[])
{
    bc_StartupSettings settings = parseArgs(argc, argv);
    chdir(settings.dataDirectory);

    // TODO: any reason to manage SDL here, instead of in basaltic_super?
    printf("Initizlizing SDL...\n");
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

    bc_startEngine(settings);

    SDL_Quit();

    return 0;
}
