#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include "basaltic_super.h"
#include "htw_core.h"

bc_StartupSettings parseArgs(int argc, char *argv[]);

bc_StartupSettings parseArgs(int argc, char *argv[]) {
    // get arg option limits
#ifdef _WIN32
    size_t maxPathLength = PATH_MAX;
#else
    size_t maxPathLength = pathconf(".", _PC_PATH_MAX);
#endif
    size_t maxStartModelArgCount = 256;
    // default settings
    bc_StartupSettings settings = {
        .startupMode = BC_STARTUP_MODE_START_MODEL,
        .enableEditor = false,
        .dataDirectory = calloc(maxPathLength, sizeof(char)),
        .loadModelPath = calloc(maxPathLength, sizeof(char)),
        .startModelArgCount = 0,
        .startModelArgs = calloc(maxStartModelArgCount, sizeof(char *)),
    };

    // default settings
    strcpy(settings.dataDirectory, "..");
    strcpy(settings.loadModelPath, "");
    //strcpy(settings.startModelArgs, "seed=6174 width=3 height=3");

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] == '-') {
            switch (arg[1]) {
                case 'n':
                    settings.startupMode = BC_STARTUP_MODE_START_MODEL;
                    i++;
                    // get next arg, break if it is a new switch
                    while ((arg = argv[i])[0] != '-') {
                        size_t arglen = strlen(arg) + 1;
                        char* dstArg = settings.startModelArgs[settings.startModelArgCount] = calloc(arglen, sizeof(char));
                        strcpy(dstArg, arg);
                        settings.startModelArgCount++;
                        // break if end of args
                        i++;
                        if (i >= argc) {
                            break;
                        }
                    }
                    i--;
                    break;
                case 'l':
                    settings.startupMode = BC_STARTUP_MODE_LOAD_MODEL;
                    strcpy(settings.loadModelPath, argv[i + 1]);
                    i++;
                    printf("Loading saved model from '%s'\n", settings.loadModelPath);
                    break;
                case 'd':
                    strcpy(settings.dataDirectory, argv[i + 1]);
                    i++;
                    break;
                case 'e':
                    settings.enableEditor = true;
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

    bc_startEngine(settings);

    return 0;
}
