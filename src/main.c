#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include "argparse.h"
#include "basaltic_defs.h"
#include "basaltic_super.h"

bc_StartupSettings parseArgs(int argc, const char *argv[]);

// FIXME: it's possible for a single argv string to contain whitespace; args might not be delivered as expeced e.g.
// Some methods of running will give argc = 3, argv = {"-n", "3", "3"}
// While others will give argc = 1, argv = {"-n 3 3"}
bc_StartupSettings parseArgs(int argc, const char *argv[]) {
    size_t maxStartModelArgCount = 256;
    // default settings
    bc_StartupSettings settings = {
        .startupMode = BC_STARTUP_MODE_START_MODEL,
        .enableEditor = false,
        .dataDirectory = NULL, //calloc(maxPathLength, sizeof(char)),
        .loadModelPath = NULL, //calloc(maxPathLength, sizeof(char)),
        .startModelArgCount = 0,
        .startModelArgs = calloc(maxStartModelArgCount, sizeof(char *)),
    };

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_BOOLEAN('e', "editor", &settings.enableEditor, "enable editor on start"),
        OPT_STRING('d', "data", &settings.dataDirectory, "game data directory to use"),
        OPT_STRING('n', "new", &settings.startModelArgs[0], "create new world on start; must specify [world size x] [world size y] in chunks, can optionally specify [seed]"),
        OPT_STRING('l', "load", &settings.loadModelPath, "!not implemented! load world at [path]"),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argparse_describe(&argparse, "Run `basaltic -h` to see launch options", "");
    int count = argparse_parse(&argparse, argc, argv);

    if (settings.startModelArgs[0] != NULL) {
        settings.startupMode = BC_STARTUP_MODE_START_MODEL;
    } else if (settings.loadModelPath != NULL) {
        settings.startupMode = BC_STARTUP_MODE_LOAD_MODEL;
    } else {
        settings.startupMode = BC_STARTUP_MODE_NO_MODEL;
    }

    return settings;
}

int main(int argc, const char *argv[])
{
    bc_StartupSettings settings = parseArgs(argc, argv);
    if (settings.dataDirectory != NULL) {
        chdir(settings.dataDirectory);
    } else {
        chdir("data/");
    }
    printf("Using data dir: %s\n", getcwd(NULL, 0));

    bc_startEngine(settings);

    return 0;
}
