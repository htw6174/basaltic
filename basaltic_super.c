
#include <SDL2/SDL.h>
#include "htw_core.h"
#include "basaltic_defs.h"
#include "basaltic_super.h"
#include "basaltic_model.h"
#include "basaltic_view.h"
#include "basaltic_super_window.h"
#include "basaltic_interaction.h"
#include "basaltic_editor_base.h"
#include "basaltic_editor.h"

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

bc_EngineSettings *loadEngineConfig(char *path);

void beginFrame(bc_WindowContext *wc);
void endFrame(bc_WindowContext *wc);

void startModel(SuperContext *sc);
void stopModel(SDL_Thread **modelThread, bc_ModelData **modelData);
void loadModel(char *path);
void continueModel();

void requestModelStop(volatile bc_ProcessState *modelThreadState);
void requestProcessStop(volatile bc_ProcessState *appState, volatile bc_ProcessState *modelThreadState);
bool isModelRunning(bc_ProcessState modelThreadState, SDL_Thread *modelThread);

int bc_startEngine(bc_StartupSettings startSettings) {

    SuperContext sc = {
        .engineConfig = loadEngineConfig("path does nothing right now!"),
        .superInterface = calloc(1, sizeof(*sc.superInterface)),

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

    sc.superInterface->signal = BC_SUPERVISOR_SIGNAL_NONE;

    u32 frameInterval = 1000 / sc.engineConfig->frameRateLimit;

    bc_WindowContext *wc = bc_createWindow(1280, 720);
    bc_EditorEngineContext editorEngineContext = bc_initEditor(startSettings.enableEditor, wc->vkContext);

    bc_ViewContext *vc = bc_view_setup(wc);
    bc_EditorContext *ec = bc_view_setupEditor();

    bool isModelPassedToView = false;
    bc_ModelData *visibleModelData = NULL;

    // launch game according to startupMode
    switch (startSettings.startupMode) {
        case BC_STARTUP_MODE_NO_MODEL:
            break;
        case BC_STARTUP_MODE_START_MODEL:
            bc_model_argsToStartSettings(startSettings.startModelArgCount, startSettings.startModelArgs, &sc.superInterface->modelSettings);
            startModel(&sc);
            break;
        case BC_STARTUP_MODE_LOAD_MODEL:
            // TODO
            //bc_loadModel(startSettings.loadGamePath);
            break;
        default:
            fprintf(stderr, "ERROR: Invalid startup mode");
            break;
    }

    while (sc.appState == BC_PROCESS_STATE_RUNNING) {
        Uint64 startTime = wc->milliSeconds = SDL_GetTicks64();

        if (sc.isModelDataReady == true && isModelPassedToView == false) {
            visibleModelData = sc.modelData;
            bc_view_onModelStart(vc, visibleModelData);
            isModelPassedToView = true;
        }

        bool passthroughMouse = !bc_editorWantCaptureMouse(&editorEngineContext);
        bool passthroughKeyboard = !bc_editorWantCaptureMouse(&editorEngineContext);
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                requestProcessStop(&sc.appState, &sc.modelThreadState);
            }
            bc_handleEditorInputEvents(&editorEngineContext, &e);
            bc_view_onInputEvent(vc, sc.inputBuffer, &e, passthroughMouse, passthroughKeyboard);
        }

        bc_view_processInputState(vc, sc.inputBuffer, passthroughMouse, passthroughKeyboard);

        beginFrame(wc);
        bc_view_drawFrame(sc.superInterface, vc, visibleModelData, wc, sc.inputBuffer);

        if (editorEngineContext.isActive) {
            bc_beginEditor();
            bc_drawEditor(&editorEngineContext, wc, superInfo);
            bc_view_drawEditor(sc.superInterface, ec, vc, visibleModelData, sc.inputBuffer);
            bc_endEditor(&editorEngineContext);
        }

        endFrame(wc);

        // handle superInterface signals
        switch (sc.superInterface->signal) {
            case BC_SUPERVISOR_SIGNAL_NONE:
                break;
            case BC_SUPERVISOR_SIGNAL_START_MODEL:
                startModel(&sc);
                break;
            case BC_SUPERVISOR_SIGNAL_STOP_MODEL:
                requestModelStop(&sc.modelThreadState);
                break;
            case BC_SUPERVISOR_SIGNAL_RESTART_MODEL:
                // TODO: do I need an option for this?
                break;
            case BC_SUPERVISOR_SIGNAL_STOP_PROCESS:
                requestProcessStop(&sc.appState, &sc.modelThreadState);
                break;
            default:
                break;
        }
        // reset signal after handling
        sc.superInterface->signal = BC_SUPERVISOR_SIGNAL_NONE;

        // if model loop has ended, wait for thread to stop and free resources
        if (sc.modelThread != NULL && sc.modelThreadState == BC_PROCESS_STATE_STOPPED) {
            isModelPassedToView = false;
            visibleModelData = NULL;
            bc_view_onModelStop(vc);
            // TODO: any reason not to reset isModelDataReady here, instead of in the model thread?
            stopModel(&sc.modelThread, &sc.modelData);
        }

        // delay until end of frame
        Uint64 endTime = SDL_GetTicks64();
        Uint64 duration = endTime - startTime;
        u64 frameMod = wc->frame % bc_frameHistoryLength;
        superInfo->frameDurations[frameMod] = duration;
        if (duration < frameInterval) {
            SDL_Delay(frameInterval - duration);
        }
        wc->frame++;
    }

    // repeated here just to be safe
    // if game loop has ended, wait for thread to stop and free resources
    if (sc.modelThread != NULL) {
        sc.modelThreadState = BC_PROCESS_STATE_STOPPED;
        stopModel(&sc.modelThread, &sc.modelData);
    }

    bc_destroyEditor(&editorEngineContext);
    bc_destroyGraphics(wc);

    return 0;
}

bc_EngineSettings *loadEngineConfig(char *path) {
    // TODO: use path to load settings from file
    bc_EngineSettings *engineConfig = calloc(1, sizeof(bc_EngineSettings));
     *engineConfig = (bc_EngineSettings){
        .frameRateLimit = 60, // TODO: figure out why changing this doesn't increase framerate above 60
        .tickRateLimit = 1000,
    };
    return engineConfig;
}

void beginFrame(bc_WindowContext *wc) {
    htw_beginFrame(wc->vkContext);
}

void endFrame(bc_WindowContext *wc) {
    htw_endFrame(wc->vkContext);
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
