
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
#include "bc_sdl_utils.h"


typedef struct {
    bc_EngineSettings *engineConfig;
    bc_SupervisorInterface *superInterface;
    bc_StartupSettings startSettings;

    bc_ProcessState appState;

    SDL_Thread *modelThread;

    bool isModelDataReady;
    bc_ModelContext modelContext;
} SuperContext;

/* Statics */
static SuperContext superContext;
static bc_WindowContext windowContext;
static bc_EditorEngineContext editorEngineContext;

static bc_SuperInfo superInfo;

static bool viewHasReceivedModel;
static u64 lastFrameStart;

bc_EngineSettings *loadEngineConfig(char *path);

void startView(bc_WindowContext *wc);
void stopView();

void startModel(int argc, char *argv[], SuperContext *sc);
void stopModel(SDL_Thread **modelThread, bc_ModelContext *modelData);
void loadModel(char *path);
void continueModel();

void requestModelStop(bc_ModelContext *modelData);
void requestProcessStop(bc_ProcessState *appState, bc_ModelContext *modelData);

static void mainLoop(void) {
    u64 frameStart = SDL_GetPerformanceCounter();

    bc_WindowContext *wc = &windowContext;
    bc_ModelContext *mctx = &superContext.modelContext;

    wc->lastFrameDuration = frameStart - lastFrameStart;
    lastFrameStart = frameStart;

    wc->milliSeconds = SDL_GetTicks64();

    if (superContext.isModelDataReady == true && viewHasReceivedModel == false) {
        bc_view_onModelStart(mctx);
        viewHasReceivedModel = true;
    }

    bool passthroughMouse = !bc_editorWantCaptureMouse(&editorEngineContext);
    bool passthroughKeyboard = !bc_editorWantCaptureKeyboard(&editorEngineContext);
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) { // Normal quit
            requestProcessStop(&superContext.appState, mctx);
        } else if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_r && (e.key.keysym.mod & KMOD_CTRL)) { // crtl+r
                // reload view
                stopView();
                startView(wc);
            }
            if (e.key.keysym.sym == SDLK_q && (e.key.keysym.mod & KMOD_CTRL)) { // crtl+q
                // Quit, or restart wasm module on web builds
#ifdef __EMSCRIPTEN__
                // TODO: restart wasm module
#else
                requestProcessStop(&superContext.appState, mctx);
#endif
            }
        } else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                bc_changeScreenSize(wc, e.window.data1, e.window.data2);
            }
        }
        bc_handleEditorInputEvents(&editorEngineContext, &e);
        bc_view_onInputEvent(&e, passthroughMouse, passthroughKeyboard);
    }

    bc_view_processInputState(passthroughMouse, passthroughKeyboard);

    bc_view_beginFrame(wc);
    bc_view_drawFrame(superContext.superInterface, wc);

    bc_beginEditor();
    if (editorEngineContext.isActive) {
        bc_drawBaseEditor(&editorEngineContext, wc, &superInfo, superContext.engineConfig);
        bc_view_drawEditor(superContext.superInterface);
    } else {
        // TODO: will eventually replace this with a gui contained in the view
        bc_view_drawGUI(superContext.superInterface);
    }
    bc_endEditor();

    bc_view_endFrame(wc);

    // handle superInterface signals
    switch (superContext.superInterface->signal) {
        case BC_SUPERVISOR_SIGNAL_NONE:
            break;
        case BC_SUPERVISOR_SIGNAL_START_MODEL:
            // TODO: figure out another way to pass model start args back from view or editor
            startModel(superContext.startSettings.startModelArgCount, superContext.startSettings.startModelArgs, &superContext);
            break;
        case BC_SUPERVISOR_SIGNAL_STOP_MODEL:
            requestModelStop(mctx);
            break;
        case BC_SUPERVISOR_SIGNAL_RESTART_MODEL:
            // TODO: do I need an option for this?
            break;
        case BC_SUPERVISOR_SIGNAL_STOP_PROCESS:
            requestProcessStop(&superContext.appState, mctx);
            break;
        default:
            break;
    }
    // reset signal after handling
    superContext.superInterface->signal = BC_SUPERVISOR_SIGNAL_NONE;

    // if model thread exists, wait for thread to stop and free resources
    if (superContext.modelThread != NULL && mctx->shouldStopModel) {
        bc_view_onModelStop(mctx);
        viewHasReceivedModel = false;
        stopModel(&superContext.modelThread, &superContext.modelContext);
    }

    // frame timings
    u64 frameEnd = SDL_GetPerformanceCounter();
    u64 duration = frameEnd - frameStart;
    u64 frameMod = wc->frame % BC_FRAME_HISTORY_LENGTH;
    superInfo.frameCPUTimes[frameMod] = duration;
    superInfo.frameTotalTimes[frameMod] = wc->lastFrameDuration;
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

    BC_SDL_REGISTER_USER_EVENTS();

    superContext = (SuperContext){
        .engineConfig = loadEngineConfig("path does nothing right now!"),
        .superInterface = calloc(1, sizeof(*superContext.superInterface)),
        .startSettings = startSettings,

        .appState = BC_PROCESS_STATE_RUNNING,

        .modelThread = NULL,
        .isModelDataReady = false,
        .modelContext = {
            .mutex = SDL_CreateMutex(),
            .cond =SDL_CreateCond(),
            .deltaTime = 1.0, // TODO: temporary until view has a mechanism to set this
        },
    };

    // TODO: track logic thread timing in module code
    //superInfo->tickDurations = calloc(bc_frameHistoryLength, sizeof(superInfo->tickDurations[0]));
    //superInfo->worldStepHistory = calloc(bc_frameHistoryLength, sizeof(superInfo->worldStepHistory[0]));

    superContext.superInterface->signal = BC_SUPERVISOR_SIGNAL_NONE;

    //u32 frameInterval = 1000 / superContext.engineConfig->frameRateLimit;

    windowContext = bc_createWindow(1280, 720);
    // TODO: rework imgui editor to use openGL backend from sokol
    editorEngineContext = bc_initEditor(startSettings.enableEditor, &windowContext);

    startView(&windowContext);

    // launch game according to startupMode
    switch (startSettings.startupMode) {
        case BC_STARTUP_MODE_NO_MODEL:
            break;
        case BC_STARTUP_MODE_START_MODEL:
            startModel(startSettings.startModelArgCount, startSettings.startModelArgs, &superContext);
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
        stopModel(&superContext.modelThread, &superContext.modelContext);
    }

    stopView();

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


void startView(bc_WindowContext *wc) {
    bc_view_setup(wc);
    bc_view_setupEditor(); // TODO: roll this into general view setup, or have an option to build without the editor
}

void stopView() {
    bc_view_teardown();
    bc_view_teardownEditor();
}

void startModel(int argc, char *argv[], SuperContext *sc) {
    sc->isModelDataReady = false;

    printf("Starting model on new thread...\n");

    // NOTE: remember to put anything being passed to another thread in heap memory
    // TODO: need to free this after thread is finished with it
    bc_ModelThreadInput *modelInput = malloc(sizeof(bc_ModelThreadInput));
    *modelInput = (bc_ModelThreadInput){
        .argc = argc,
        .argv = argv,
        .isModelDataReady = &sc->isModelDataReady,
        .modelContext = &sc->modelContext,
    };
    sc->modelThread = SDL_CreateThread(bc_model_run, "model", modelInput);
}

void stopModel(SDL_Thread **modelThread, bc_ModelContext *modelData) {
    requestModelStop(modelData);
    int modelThreadResult;
    SDL_WaitThread(*modelThread, &modelThreadResult);
    *modelThread = NULL;
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

void requestModelStop(bc_ModelContext *modelData) {
    modelData->shouldStopModel = true;
    modelData->runForSteps = 0;
    SDL_CondSignal(modelData->cond);
}

void requestProcessStop(bc_ProcessState *appState, bc_ModelContext *modelData) {
    *appState = BC_PROCESS_STATE_STOPPED;
    requestModelStop(modelData);
}
