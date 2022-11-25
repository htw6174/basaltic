#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
    bt_LogicInputState *input;
    bt_WorldState *world;
    Uint32 interval;
    KD_APPSTATE *volatile appState;
} LogicLoopInput;

// started as a seperate thread by SDL
int logicLoop(void *ptr) { // TODO: can this be a specific input type? If so, change to LogicLoopInput*
    // Extract input data
    LogicLoopInput *loopInput = (LogicLoopInput*)ptr;
    bt_LogicInputState *input = loopInput->input;
    bt_WorldState *world = loopInput->world;
    Uint32 interval = loopInput->interval;
    KD_APPSTATE *appState = loopInput->appState;

    while (*appState == KD_APPSTATE_RUNNING) {
        Uint64 startTime = SDL_GetTicks64();

        bt_simulateWorld(input, world);

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
int interactiveWindowLoop (bt_UiState *ui, bt_LogicInputState *logicInput, bt_WorldState *world, Uint32 interval, KD_APPSTATE *volatile appState) {
    /* for testing SDL direct draw
    Uint32 terrainFormat = SDL_PIXELFORMAT_RGBA32;
    Uint32 *bmp = valueMapToBitmap(world->heightMap, NULL, terrainFormat);
    SDL_Surface *terrain = NULL;
    int width = world->heightMap->width;
    int height = world->heightMap->height;
    terrain = SDL_CreateRGBSurfaceWithFormatFrom(bmp, width, height, sizeof(Uint32) / 8, width * sizeof(Uint32), terrainFormat);
    */

    // TODO: should allocate heap space for these large structures instead of leaving them on the stack. Also: should I init by passing pointer to an existing struct, or returning the whole thing?
    bt_GraphicsState graphics;
    bt_initGraphics(&graphics, 1280, 720);
    bt_initWorldGraphics(&graphics, world);
    bt_EditorContext editorContext = bt_initEditor(graphics.vkContext);

    while (*appState == KD_APPSTATE_RUNNING) {
        Uint64 startTime = graphics.milliSeconds = SDL_GetTicks64();

        bool passthroughMouse = !bt_editorWantCaptureMouse(&editorContext);
        bool passthroughKeyboard = !bt_editorWantCaptureMouse(&editorContext);
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                *appState = KD_APPSTATE_STOPPED;
            }
            bt_handleEditorInputEvents(&editorContext, &e);
            bt_processInputEvent(ui, logicInput, &e, passthroughMouse, passthroughKeyboard);
        }

        bt_processInputState(ui, logicInput, passthroughMouse, passthroughKeyboard);

        bt_drawFrame(&graphics, ui, world);
        bt_drawEditor(&editorContext, &graphics, ui, world);
        bt_endFrame(&graphics);

        // delay until end of frame
        Uint64 endTime = SDL_GetTicks64();
        Uint64 duration = endTime - startTime;
        if (duration < interval) {
            SDL_Delay(interval - duration);
        }
        graphics.frame++;
    }

    bt_destroyEditor(&editorContext);
    bt_destroyGraphics(&graphics);

    return 0;
}

int main(int argc, char *argv[])
{
    //loadTileDefinitions ("resources/cell_types");

    // Must indicate where used that the value of this is volatile, as it may get updated by another thread
    KD_APPSTATE appState = KD_APPSTATE_RUNNING;

    printf("Initizlizing SDL...\n");
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

    bt_UiState ui = bt_createUiState();
    bt_WorldState *world = bt_createWorldState(8, 8, 64, 64);
    bt_initializeWorldState(world);
    bt_LogicInputState *logicInput = createLogicInputState();

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
