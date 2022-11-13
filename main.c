#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include "kingdom_defs.h"
#include "kingdom_logic.h"
#include "kingdom_window.h"
#include "kingdom_interaction.h"
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

typedef struct {
    kd_LogicInputState *input;
    kd_WorldState *world;
    Uint32 interval;
    KD_APPSTATE *volatile appState;
} LogicLoopInput;

// started as a seperate thread by SDL
int logicLoop(void *ptr) { // TODO: can this be a specific input type? If so, change to LogicLoopInput*
    // Extract input data
    LogicLoopInput *loopInput = (LogicLoopInput*)ptr;
    kd_LogicInputState *input = loopInput->input;
    kd_WorldState *world = loopInput->world;
    Uint32 interval = loopInput->interval;
    KD_APPSTATE *appState = loopInput->appState;

    while (*appState == KD_APPSTATE_RUNNING) {
        Uint64 startTime = SDL_GetTicks64();

        kd_simulateWorld(input, world);

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
int interactiveWindowLoop (kd_UiState *ui, kd_LogicInputState *logicInput, kd_WorldState *world, Uint32 interval, KD_APPSTATE *volatile appState) {
    /* for testing SDL direct draw
    Uint32 terrainFormat = SDL_PIXELFORMAT_RGBA32;
    Uint32 *bmp = valueMapToBitmap(world->heightMap, NULL, terrainFormat);
    SDL_Surface *terrain = NULL;
    int width = world->heightMap->width;
    int height = world->heightMap->height;
    terrain = SDL_CreateRGBSurfaceWithFormatFrom(bmp, width, height, sizeof(Uint32) / 8, width * sizeof(Uint32), terrainFormat);
    */

    kd_GraphicsState graphics;
    kd_InitGraphics(&graphics, 1280, 720);
    kd_initWorldGraphics(&graphics, world);

    while (*appState == KD_APPSTATE_RUNNING) {
        Uint64 startTime = graphics.milliSeconds = SDL_GetTicks64();

        kd_handleInputs(ui, logicInput, appState);

        kd_renderFrame(&graphics, ui, world);

        // delay until end of frame
        Uint64 endTime = SDL_GetTicks64();
        Uint64 duration = endTime - startTime;
        if (duration < interval) {
            SDL_Delay(interval - duration);
        }
        graphics.frame++;
    }

    kd_DestroyGraphics(&graphics);

    return 0;
}

int main(int argc, char *argv[])
{
    //loadTileDefinitions ("resources/cell_types");

    // Must indicate where used that the value of this is volatile, as it may get updated by another thread
    KD_APPSTATE appState = KD_APPSTATE_RUNNING;

    printf("Initizlizing SDL...\n");
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

    kd_UiState ui = kd_createUiState();
    kd_WorldState *world = kd_createWorldState(8, 8, 64, 64);
    kd_initializeWorldState(world);
    kd_LogicInputState *logicInput = createLogicInputState();

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
