
#include <SDL2/SDL.h>
#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_super.h"
#include "basaltic_logic.h"
#include "basaltic_window.h"
#include "basaltic_interaction.h"
#include "basaltic_editor.h"

// Must indicate where used that the value of this is volatile, as it may get updated by another thread
// NOTE: `volatile int *foo` -> pointer to the volatile int foo, while `int *volatile foo` -> volatile pointer to NON-volatile int foo
static volatile bc_ProcessState appState = BC_PROCESS_STATE_STOPPED;
static volatile bc_ProcessState logicThreadState = BC_PROCESS_STATE_STOPPED;

static SDL_Thread *logicThread = NULL;

static bc_EngineSettings *engineConfig = NULL;
static bc_SuperInfo *superInfo = NULL;

static bc_UiState ui;

static bc_WorldState *world = NULL;
static bc_CommandQueue worldInputQueue;

void loadEngineConfig(char *path);
void startGame();
void endGame();

int bc_startEngine(bc_StartupSettings startSettings) {

    appState = BC_PROCESS_STATE_RUNNING;
    logicThreadState = BC_PROCESS_STATE_STOPPED;

    // TODO: load config from configurable path
    loadEngineConfig("does nothing right now");

    superInfo = calloc(1, sizeof(bc_SuperInfo));
    superInfo->frameDurations = calloc(bc_frameHistoryLength, sizeof(superInfo->frameDurations[0]));
    superInfo->tickDurations = calloc(bc_frameHistoryLength, sizeof(superInfo->tickDurations[0]));
    superInfo->worldStepHistory = calloc(bc_frameHistoryLength, sizeof(superInfo->worldStepHistory[0]));

    u32 frameInterval = 1000 / engineConfig->frameRateLimit;

    // TODO: should allocate heap space for these large structures instead of leaving them on the stack. Also: should I init by passing pointer to an existing struct, or returning the whole thing?
    ui = bc_createUiState();
    bc_GraphicsState graphics;
    bc_initGraphics(&graphics, 1280, 720);
    bc_EditorContext editorContext = bc_initEditor(startSettings.enableEditor, graphics.vkContext);

    worldInputQueue = bc_createCommandQueue(bc_commandQueueMaxSize);

    // launch game according to startupMode
    switch (startSettings.startupMode) {
        case BC_STARTUP_MODE_MAINMENU:
            ui.interfaceMode = BC_INTERFACE_MODE_SYSTEM_MENU;
            break;
        case BC_STARTUP_MODE_NEWGAME:
            ui.interfaceMode = BC_INTERFACE_MODE_GAMEPLAY;
            bc_startNewGame(3, 3, startSettings.newGameSeed);
            break;
        case BC_STARTUP_MODE_CONTINUEGAME:
            // TODO: set load game path from most recent save
        case BC_STARTUP_MODE_LOADGAME:
            ui.interfaceMode = BC_INTERFACE_MODE_GAMEPLAY;
            bc_loadGame(startSettings.loadGamePath);
            break;
        default:
            fprintf(stderr, "ERROR: Invalid startup mode");
            break;
    }

    while (appState == BC_PROCESS_STATE_RUNNING) {
        Uint64 startTime = graphics.milliSeconds = SDL_GetTicks64();

        bool passthroughMouse = !bc_editorWantCaptureMouse(&editorContext);
        bool passthroughKeyboard = !bc_editorWantCaptureMouse(&editorContext);
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                bc_requestProcessStop();
            }
            bc_handleEditorInputEvents(&editorContext, &e);
            bc_processInputEvent(&ui, worldInputQueue, &e, passthroughMouse, passthroughKeyboard);
        }

        bc_processInputState(&ui, worldInputQueue, passthroughMouse, passthroughKeyboard);

        bc_drawFrame(&graphics, &ui, world);
        bc_drawEditor(&editorContext, superInfo, &graphics, &ui, world, worldInputQueue);
        bc_endFrame(&graphics);

        // if game loop has ended, wait for thread to stop and free resources
        if (logicThread != NULL && logicThreadState == BC_PROCESS_STATE_STOPPED) {
            // TODO: need a better standard for how interface modes will change as the game starts and stops. Right now is spread between _super and _editor. Should also have more null checking when referencing world or logic input state, as these will frequently be NULL
            ui.interfaceMode = BC_INTERFACE_MODE_SYSTEM_MENU;
            endGame();
        }

        // delay until end of frame
        Uint64 endTime = SDL_GetTicks64();
        Uint64 duration = endTime - startTime;
        u64 frameMod = graphics.frame % bc_frameHistoryLength;
        superInfo->frameDurations[frameMod] = duration;
        if (world != NULL) {
            superInfo->worldStepHistory[frameMod] = world->step;
        }
        if (duration < frameInterval) {
            SDL_Delay(frameInterval - duration);
        }
        graphics.frame++;
    }

    // repeated here just to be safe
    // if game loop has ended, wait for thread to stop and free resources
    if (logicThread != NULL) {
        logicThreadState = BC_PROCESS_STATE_STOPPED;
        endGame();
    }

    bc_destroyEditor(&editorContext);
    bc_destroyGraphics(&graphics);

    return 0;
}

void loadEngineConfig(char *path) {
    engineConfig = calloc(1, sizeof(bc_EngineSettings));
     *engineConfig = (bc_EngineSettings){
        .frameRateLimit = 60, // TODO: figure out why changing this doesn't increase framerate above 60
        .tickRateLimit = 1000,
    };
}

void startGame() {
    u32 tickInterval = 1000 / engineConfig->tickRateLimit;

    bc_SetCameraWrapLimits(&ui, world->worldWidth, world->worldHeight);

    // NOTE: remember to put anything being passed to another thread on the heap
    bc_LogicThreadInput *logicLoopParams = malloc(sizeof(bc_LogicThreadInput));
    *logicLoopParams = (bc_LogicThreadInput){
        .world = world,
        .inputQueue = worldInputQueue,
        .interval = tickInterval,
        .threadState = &logicThreadState};
    logicThread = SDL_CreateThread(bc_runLogicThread, "logic", logicLoopParams);
}

void endGame() {
    int gameplayThreadResult;
    SDL_WaitThread(logicThread, &gameplayThreadResult);
    // TODO: reset uiState that references world data
    bc_destroyWorldState(world);
    world = NULL;
    logicThread = NULL;
}

void bc_startNewGame(u32 width, u32 height, char *seed) {
    logicThreadState = BC_PROCESS_STATE_RUNNING;

    world = bc_createWorldState(width, height, seed);
    bc_initializeWorldState(world);
    // TODO: set interface mode and active character
    startGame();
}

void bc_loadGame(char *path) {
    // TODO
}

void bc_continueGame() {
    // TODO
}

void bc_requestGameStop() {
    logicThreadState = BC_PROCESS_STATE_STOPPED;
}

void bc_requestProcessStop() {
    appState = BC_PROCESS_STATE_STOPPED;
    logicThreadState = BC_PROCESS_STATE_STOPPED;
}

bool bc_isGameRunning() {
    return (logicThreadState != BC_PROCESS_STATE_STOPPED) || (logicThread != NULL);
}
