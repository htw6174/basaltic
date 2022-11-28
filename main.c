#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>
#include "basaltic_defs.h"
#include "basaltic_logic.h"
#include "basaltic_window.h"
#include "basaltic_interaction.h"
#include "basaltic_editor.h"
#include "htw_core.h"
#include "htw_vulkan.h"

#define LOGIC_TPS 30
#define RENDER_TPS 60

/* testing stuff, should be moved to another file
void testMap() {
    TileMap *myTMap = createTileMap (15, 5);
    setMapTile ( myTMap, ( MapTile ){5, 0}, 14, 4);
    printTileMap ( myTMap );

    putchar('\n');

    ValueMap *myVMap = createValueMap (20, 6, 9);
    fillGradient(myVMap, 10, 0);
    printValueMap(myVMap);
}

// If bmp is NULL, a new bitmap of appropriate size is allocated. Retuns a pointer to bmp.
Uint32 *valueMapToBitmap(ValueMap *map, Uint32 *bmp, Uint32 format) {
    int mapSize = map->width * map->height;
    if (bmp == NULL)
        bmp = malloc(mapSize * sizeof(Uint32));
    for (int i = 0; i < mapSize; i++) {
        int value = map->values[i];
        //value = remap_int(0, map->maxValue, 0, 255, value);
        SDL_PixelFormat *pixelFormat = SDL_AllocFormat(format);
        bmp[i] = SDL_MapRGB(pixelFormat, value, 0, 0);
    }
    return bmp;
}
*/

typedef enum StartupMode {
    STARTUP_MODE_MAINMENU,
    STARTUP_MODE_NEWGAME,
    STARTUP_MODE_LOADGAME,
    STARTUP_MODE_CONTINUEGAME
} StartupMode;

typedef struct {
    StartupMode startupMode;
    char *dataDirectory;
    char *loadGamePath;
    char *newGameSeed;
} ArgSettings;

typedef struct {
    bc_LogicInputState *input;
    bc_WorldState *world;
    Uint32 interval;
    BC_APPSTATE *volatile appState;
} LogicLoopInput;

ArgSettings parseArgs(int argc, char *argv[]);
// started as a seperate thread by SDL
int logicLoop(void *ptr);
int interactiveWindowLoop (bc_UiState *ui, bc_LogicInputState *logicInput, bc_WorldState *world, Uint32 interval, BC_APPSTATE *volatile appState);

ArgSettings parseArgs(int argc, char *argv[]) {
    // default settings
    ArgSettings settings = {
        .startupMode = STARTUP_MODE_MAINMENU,
        .dataDirectory = "..",
        .loadGamePath = "",
        .newGameSeed = "6174",
    };

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] == '-') {
            switch (arg[1]) {
                case 'n':
                    settings.startupMode = STARTUP_MODE_NEWGAME;
                    settings.newGameSeed = argv[i + 1];
                    i++;
                    printf("Starting new game with seed '%s'\n", settings.newGameSeed);
                    break;
                case 'l':
                    settings.startupMode = STARTUP_MODE_LOADGAME;
                    settings.loadGamePath = argv[i + 1];
                    i++;
                    printf("Loading save '%s'\n", settings.loadGamePath);
                    break;
                case 'c':
                    settings.startupMode = STARTUP_MODE_CONTINUEGAME;
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

int logicLoop(void *ptr) { // TODO: can this be a specific input type? If so, change to LogicLoopInput*
    // Extract input data
    LogicLoopInput *loopInput = (LogicLoopInput*)ptr;
    bc_LogicInputState *input = loopInput->input;
    bc_WorldState *world = loopInput->world;
    Uint32 interval = loopInput->interval;
    BC_APPSTATE *appState = loopInput->appState;

    while (*appState == BC_APPSTATE_RUNNING) {
        Uint64 startTime = SDL_GetTicks64();

        bc_simulateWorld(input, world);

        // delay until end of frame
        Uint64 endTime = SDL_GetTicks64();
        Uint64 duration = endTime - startTime;
        if (duration < interval) {
            SDL_Delay(interval - duration);
        }
    }
    return 0;
}

// must run on the main thread
int interactiveWindowLoop (bc_UiState *ui, bc_LogicInputState *logicInput, bc_WorldState *world, Uint32 interval, BC_APPSTATE *volatile appState) {
    /* for testing SDL direct draw
    Uint32 terrainFormat = SDL_PIXELFORMAT_RGBA32;
    Uint32 *bmp = valueMapToBitmap(world->heightMap, NULL, terrainFormat);
    SDL_Surface *terrain = NULL;
    int width = world->heightMap->width;
    int height = world->heightMap->height;
    terrain = SDL_CreateRGBSurfaceWithFormatFrom(bmp, width, height, sizeof(Uint32) / 8, width * sizeof(Uint32), terrainFormat);
    */

    // TODO: should allocate heap space for these large structures instead of leaving them on the stack. Also: should I init by passing pointer to an existing struct, or returning the whole thing?
    bc_GraphicsState graphics;
    bc_initGraphics(&graphics, 1280, 720);
    bc_initWorldGraphics(&graphics, world);
    bc_EditorContext editorContext = bc_initEditor(graphics.vkContext);

    while (*appState == BC_APPSTATE_RUNNING) {
        Uint64 startTime = graphics.milliSeconds = SDL_GetTicks64();

        bool passthroughMouse = !bc_editorWantCaptureMouse(&editorContext);
        bool passthroughKeyboard = !bc_editorWantCaptureMouse(&editorContext);
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                *appState = BC_APPSTATE_STOPPED;
            }
            bc_handleEditorInputEvents(&editorContext, &e);
            bc_processInputEvent(ui, logicInput, &e, passthroughMouse, passthroughKeyboard);
        }

        bc_processInputState(ui, logicInput, passthroughMouse, passthroughKeyboard);

        bc_drawFrame(&graphics, ui, world);
        bc_drawEditor(&editorContext, &graphics, ui, world);
        bc_endFrame(&graphics);

        // delay until end of frame
        Uint64 endTime = SDL_GetTicks64();
        Uint64 duration = endTime - startTime;
        if (duration < interval) {
            SDL_Delay(interval - duration);
        }
        graphics.frame++;
    }

    bc_destroyEditor(&editorContext);
    bc_destroyGraphics(&graphics);

    return 0;
}

int main(int argc, char *argv[])
{
    ArgSettings settings = parseArgs(argc, argv);
    chdir(settings.dataDirectory);
    //loadTileDefinitions ("resources/cell_types");

    // Must indicate where used that the value of this is volatile, as it may get updated by another thread
    BC_APPSTATE appState = BC_APPSTATE_RUNNING;

    printf("Initizlizing SDL...\n");
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

    bc_UiState ui = bc_createUiState();
    bc_WorldState *world = bc_createWorldState(8, 8, 64, 64);
    bc_initializeWorldState(world);
    bc_LogicInputState *logicInput = createLogicInputState();

    Uint32 logicInterval = 1000 / LOGIC_TPS;
    Uint32 frameInterval = 1000 / RENDER_TPS;

    LogicLoopInput logicLoopParams = {logicInput, world, logicInterval, &appState};
    SDL_Thread *logicThread = SDL_CreateThread(logicLoop, "logic", &logicLoopParams);
    int logicThreadResult;

    interactiveWindowLoop(&ui, logicInput, world, frameInterval, &appState);

    SDL_WaitThread(logicThread, &logicThreadResult);

    SDL_Quit();

    return 0;
}
