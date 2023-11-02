
#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_super.h"
#include "basaltic_model.h"
#include "basaltic_view.h"
#include "basaltic_super_window.h"
#include "basaltic_editor_base.h"

static bc_SuperInfo *superInfo = NULL;

typedef struct {
    bc_EngineSettings *engineConfig;
    bc_SupervisorInterface *superInterface;

    // Must indicate where used that the value of this is volatile, as it may get updated by another thread
    // NOTE: `volatile int *foo` -> pointer to the volatile int foo, while `int *volatile foo` -> volatile pointer to NON-volatile int foo
    volatile bc_ProcessState appState;
    volatile bc_ProcessState modelThreadState;

    SDL_Thread *modelThread;

    bool isModelDataReady;
    bc_ModelData *modelData;
    bc_CommandBuffer inputBuffer;
} SuperContext;

/* Statics */
static SuperContext superContext;
static bc_WindowContext windowContext;
static bc_EditorEngineContext editorEngineContext;

static bool viewHasReceivedModel;
static u64 lastFrameStart;

bc_EngineSettings *loadEngineConfig(char *path);

void startModel(SuperContext *sc);
void stopModel(SDL_Thread **modelThread, bc_ModelData **modelData);
void loadModel(char *path);
void continueModel();

void requestModelStop(volatile bc_ProcessState *modelThreadState);
void requestProcessStop(volatile bc_ProcessState *appState, volatile bc_ProcessState *modelThreadState);
bool isModelRunning(bc_ProcessState modelThreadState, SDL_Thread *modelThread);

static void mainLoop(void) {
    u64 frameStart = SDL_GetPerformanceCounter();

    bc_WindowContext *wc = &windowContext;
    bc_ModelData *md = superContext.modelData;

    wc->lastFrameDuration = frameStart - lastFrameStart;
    lastFrameStart = frameStart;

    wc->milliSeconds = SDL_GetTicks64();

    if (superContext.isModelDataReady == true && viewHasReceivedModel == false) {
        md = superContext.modelData;
        bc_view_onModelStart(md);
        viewHasReceivedModel = true;
    }

    bool passthroughMouse = !bc_editorWantCaptureMouse(&editorEngineContext);
    bool passthroughKeyboard = !bc_editorWantCaptureKeyboard(&editorEngineContext);
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            requestProcessStop(&superContext.appState, &superContext.modelThreadState);
        } else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                bc_changeScreenSize(wc, e.window.data1, e.window.data2);
            }
        }
        bc_handleEditorInputEvents(&editorEngineContext, &e);
        bc_view_onInputEvent(superContext.inputBuffer, &e, passthroughMouse, passthroughKeyboard);
    }

    bc_view_processInputState(superContext.inputBuffer, passthroughMouse, passthroughKeyboard);

    bc_view_beginFrame(wc);
    bc_view_drawFrame(superContext.superInterface, md, wc, superContext.inputBuffer);

    if (editorEngineContext.isActive) {
        bc_beginEditor();
        bc_drawBaseEditor(&editorEngineContext, wc, superInfo, superContext.engineConfig);
        bc_view_drawEditor(superContext.superInterface, md, superContext.inputBuffer);
        bc_endEditor();
    }

    bc_view_endFrame(wc);

    // handle superInterface signals
    switch (superContext.superInterface->signal) {
        case BC_SUPERVISOR_SIGNAL_NONE:
            break;
        case BC_SUPERVISOR_SIGNAL_START_MODEL:
            startModel(&superContext);
            break;
        case BC_SUPERVISOR_SIGNAL_STOP_MODEL:
            requestModelStop(&superContext.modelThreadState);
            break;
        case BC_SUPERVISOR_SIGNAL_RESTART_MODEL:
            // TODO: do I need an option for this?
            break;
        case BC_SUPERVISOR_SIGNAL_STOP_PROCESS:
            requestProcessStop(&superContext.appState, &superContext.modelThreadState);
            break;
        default:
            break;
    }
    // reset signal after handling
    superContext.superInterface->signal = BC_SUPERVISOR_SIGNAL_NONE;

    // if model loop has ended, wait for thread to stop and free resources
    if (superContext.modelThread != NULL && superContext.modelThreadState == BC_PROCESS_STATE_STOPPED) {
        viewHasReceivedModel = false;
        md = NULL;
        bc_view_onModelStop();
        // TODO: any reason not to reset isModelDataReady here, instead of in the model thread?
        stopModel(&superContext.modelThread, &superContext.modelData);
    }

    // frame timings
    u64 frameEnd = SDL_GetPerformanceCounter();
    u64 duration = frameEnd - frameStart;
    u64 frameMod = wc->frame % bc_frameHistoryLength;
    superInfo->frameDurations[frameMod] = duration;
    wc->frame++;

    // no need to delay with vsync enabled
    SDL_GL_SwapWindow(wc->window);

#ifdef __EMSCRIPTEN__
    if (superContext.appState == BC_PROCESS_STATE_STOPPED) {
        emscripten_cancel_main_loop();
    }
#endif
}

int bc_startEngine(bc_StartupSettings startSettings) {

    printf("Initizlizing SDL...\n");
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO) != 0) {
        printf("Error initializing SDL: %s\n", SDL_GetError());
    }

    superContext = (SuperContext){
        .engineConfig = loadEngineConfig("path does nothing right now!"),
        .superInterface = calloc(1, sizeof(*superContext.superInterface)),

        .appState = BC_PROCESS_STATE_RUNNING,
        .modelThreadState = BC_PROCESS_STATE_STOPPED,

        .modelThread = NULL,

        .isModelDataReady = false,
        .modelData = NULL,
        .inputBuffer = bc_createCommandBuffer(bc_commandBufferMaxCommandCount, bc_commandBufferMaxArenaSize),
    };

    superInfo = calloc(1, sizeof(superInfo));
    // TODO: move to coreEditor
    superInfo->frameDurations = calloc(bc_frameHistoryLength, sizeof(superInfo->frameDurations[0]));
    // TODO: track logic thread timing in module code
    //superInfo->tickDurations = calloc(bc_frameHistoryLength, sizeof(superInfo->tickDurations[0]));
    //superInfo->worldStepHistory = calloc(bc_frameHistoryLength, sizeof(superInfo->worldStepHistory[0]));

    superContext.superInterface->signal = BC_SUPERVISOR_SIGNAL_NONE;

    //u32 frameInterval = 1000 / superContext.engineConfig->frameRateLimit;

    windowContext = bc_createWindow(1280, 720);
    // TODO: rework imgui editor to use openGL backend from sokol
    editorEngineContext = bc_initEditor(startSettings.enableEditor, &windowContext);

    bc_view_setup(&windowContext);
    bc_view_setupEditor(); // TODO: roll this into general view setup, or have an option to build without the editor

    bool isModelPassedToView = false;
    // TODO: consider rearchitecting this to support multiple model instances running simultainously, allow view to see any/all of them
    superContext.modelData = NULL;

    // launch game according to startupMode
    switch (startSettings.startupMode) {
        case BC_STARTUP_MODE_NO_MODEL:
            break;
        case BC_STARTUP_MODE_START_MODEL:
            bc_model_argsToStartSettings(startSettings.startModelArgCount, startSettings.startModelArgs, &superContext.superInterface->modelSettings);
            startModel(&superContext);
            break;
        case BC_STARTUP_MODE_LOAD_MODEL:
            // TODO
            //bc_loadModel(startSettings.loadGamePath);
            break;
        default:
            fprintf(stderr, "ERROR: Invalid startup mode");
            break;
    }

    lastFrameStart = SDL_GetPerformanceCounter();

#ifdef __EMSCRIPTEN__
    printf("Starting Emscripten main loop...\n");
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (superContext.appState == BC_PROCESS_STATE_RUNNING) {
        mainLoop();
    }
#endif

    // repeated here just to be safe
    // if game loop has ended, wait for thread to stop and free resources
    if (superContext.modelThread != NULL) {
        superContext.modelThreadState = BC_PROCESS_STATE_STOPPED;
        stopModel(&superContext.modelThread, &superContext.modelData);
    }

    bc_destroyEditor(&editorEngineContext);
    bc_destroyWindow(&windowContext);

    SDL_Quit();

    return 0;
}

bc_EngineSettings *loadEngineConfig(char *path) {
    // TODO: use path to load settings from file
    bc_EngineSettings *engineConfig = calloc(1, sizeof(bc_EngineSettings));
     *engineConfig = (bc_EngineSettings){
        .frameRateLimit = 120,
        .tickRateLimit = 100,
    };
    return engineConfig;
}

void startModel(SuperContext *sc) {
    u32 tickInterval = 1000 / sc->engineConfig->tickRateLimit;
    sc->isModelDataReady = false;

    // NOTE: remember to put anything being passed to another thread in heap memory
    // TODO: need to free this after thread is finished with it
    bc_ModelThreadInput *logicLoopParams = malloc(sizeof(bc_ModelThreadInput));
    *logicLoopParams = (bc_ModelThreadInput){
        .inputBuffer = sc->inputBuffer,
        .interval = tickInterval,
        .threadState = &sc->modelThreadState,
        .modelSettings = &sc->superInterface->modelSettings,
        .isModelDataReady = &sc->isModelDataReady,
        .modelDataRef = &sc->modelData,
    };
    sc->modelThread = SDL_CreateThread(bc_model_start, "logic", logicLoopParams);
    sc->modelThreadState = BC_PROCESS_STATE_RUNNING;
}

void stopModel(SDL_Thread **modelThread, bc_ModelData **modelData) {
    int modelThreadResult;
    SDL_WaitThread(*modelThread, &modelThreadResult);
    *modelThread = NULL;

    bc_model_cleanup(*modelData);
    *modelData = NULL;
}

// void bc_startNewGame(u32 width, u32 height, char *seed) {
//     logicThreadState = BC_PROCESS_STATE_RUNNING;
//     // TODO: set interface mode and active character
//     startModel();
// }

void loadModel(char *path) {
    // TODO
}

void continueModel() {
    // TODO
}

void requestModelStop(volatile bc_ProcessState *modelThreadState) {
    *modelThreadState = BC_PROCESS_STATE_STOPPED;
}

void requestProcessStop(volatile bc_ProcessState *appState, volatile bc_ProcessState *modelThreadState) {
    *appState = BC_PROCESS_STATE_STOPPED;
    *modelThreadState = BC_PROCESS_STATE_STOPPED;
}

bool isModelRunning(bc_ProcessState modelThreadState, SDL_Thread *modelThread) {
    return (modelThreadState != BC_PROCESS_STATE_STOPPED) || (modelThread != NULL);
}
