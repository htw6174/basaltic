
#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_super.h"
#include "basaltic_logic.h"
#include "basaltic_window.h"
#include "basaltic_interaction.h"
#include "basaltic_editor.h"

typedef struct {
    bc_LogicInputState *input;
    bc_WorldState *world;
    Uint32 interval;
    volatile bc_AppState *appState;
} LogicLoopInput;

SDL_Thread* startGame(volatile bc_AppState *appState, const bc_EngineSettings *config, bc_LogicInputState **logicInput, bc_WorldState **world);
// started as a seperate thread by SDL
int logicLoop(void *ptr);

// TODO: create logic thread inside the main loop
int bc_startEngine(bc_StartupSettings startSettings) {

    // Must indicate where used that the value of this is volatile, as it may get updated by another thread
    // NOTE: `volatile int *foo` -> pointer to the volatile int foo, while `int *volatile foo` -> volatile pointer to NON-volatile int foo
    volatile bc_AppState appState = BC_APPSTATE_RUNNING;

    // TODO: load config
    bc_EngineSettings config = {
        .frameRateLimit = 60,
        .tickRateLimit = 300,
    };

    u32 frameInterval = 1000 / config.frameRateLimit;

    // TODO: should allocate heap space for these large structures instead of leaving them on the stack. Also: should I init by passing pointer to an existing struct, or returning the whole thing?
    bc_UiState ui = bc_createUiState();
    bc_GraphicsState graphics;
    bc_initGraphics(&graphics, 1280, 720);
    bc_EditorContext editorContext = bc_initEditor(startSettings.enableEditor, graphics.vkContext);

    bc_LogicInputState *logicInput = NULL;
    bc_WorldState *world = NULL;

    SDL_Thread *gameplayThread = NULL;
    int gameplayThreadResult;

    while (appState == BC_APPSTATE_RUNNING) {
        Uint64 startTime = graphics.milliSeconds = SDL_GetTicks64();

        // start gameplay thread on game start no gameplay thread is already active
        if (gameplayThread == NULL && ui.interfaceMode != BC_INTERFACE_MODE_SYSTEM_MENU) {
            gameplayThread = startGame(&appState, &config, &logicInput, &world);
        }

        bool passthroughMouse = !bc_editorWantCaptureMouse(&editorContext);
        bool passthroughKeyboard = !bc_editorWantCaptureMouse(&editorContext);
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                appState = BC_APPSTATE_STOPPED;
            }
            bc_handleEditorInputEvents(&editorContext, &e);
            bc_processInputEvent(&ui, logicInput, &e, passthroughMouse, passthroughKeyboard);
        }

        bc_processInputState(&ui, logicInput, passthroughMouse, passthroughKeyboard);

        bc_drawFrame(&graphics, &ui, world);
        bc_drawEditor(&editorContext, &graphics, &ui, world);
        bc_endFrame(&graphics);

        // delay until end of frame
        Uint64 endTime = SDL_GetTicks64();
        Uint64 duration = endTime - startTime;
        if (duration < frameInterval) {
            SDL_Delay(frameInterval - duration);
        }
        graphics.frame++;

        // TODO: this isn't quite right. Need to use a different appstate value for the gameplay thread and main application, and only wait on this thread if the gameplay thread appstate is stopped. Will also need to ensure that any subprocess appstates are superceded by the main application's appstate
        if (gameplayThread != NULL && ui.interfaceMode == BC_INTERFACE_MODE_SYSTEM_MENU) {
            SDL_WaitThread(gameplayThread, &gameplayThreadResult);
        }
    }

    bc_destroyEditor(&editorContext);
    bc_destroyGraphics(&graphics);

    return 0;
}

int logicLoop(void *ptr) { // TODO: can this be a specific input type? If so, change to LogicLoopInput*
    // Extract input data
    LogicLoopInput *loopInput = (LogicLoopInput*)ptr;
    bc_LogicInputState *input = loopInput->input;
    bc_WorldState *world = loopInput->world;
    Uint32 interval = loopInput->interval;
    volatile bc_AppState *appState = loopInput->appState;

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

SDL_Thread* startGame(volatile bc_AppState *appState, const bc_EngineSettings *config, bc_LogicInputState **logicInput, bc_WorldState **world) {
    *logicInput = bc_createLogicInputState();
    *world = bc_createWorldState(8, 8, bc_chunkSize, bc_chunkSize); // TODO: remove the chunk size params, they should be globally constant
    bc_initializeWorldState(*world);

    u32 tickInterval = 1000 / config->tickRateLimit;

    // NOTE: remember to put anything being passed to another thread on the heap
    LogicLoopInput *logicLoopParams = malloc(sizeof(LogicLoopInput));
    *logicLoopParams = (LogicLoopInput){
        .input = *logicInput,
        .world = *world,
        .interval = tickInterval,
        .appState = appState};
    SDL_Thread *logicThread = SDL_CreateThread(logicLoop, "logic", logicLoopParams);
    return logicThread;
}

void bc_startNewGame(char *seed) {

    int logicThreadResult;
}
