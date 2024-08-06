
#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_editor_base.h"
#include "basaltic_super.h"
#include "basaltic_view.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_OPENGL3
#define CIMGUI_USE_SDL2
#include "cimgui.h"
#include "cimgui_impl.h"

#ifdef __EMSCRIPTEN__
#define GL_VERSION "#version 300 es"
#else
#define GL_VERSION "#version 330"
#endif

bc_EditorEngineContext bc_initEditor(bool isActiveAtStart, bc_WindowContext *wc) {
    // TODO: imgui saves imgui.ini in the cwd by default, which will usually be the data folder for this project. Consider changing it to a more useful default by setting io.IniFileName
    printf("Initializing cimgui SDL+OpenGL with glsl %s\n", GL_VERSION);
    igCreateContext(NULL);
    ImGui_ImplSDL2_InitForOpenGL(wc->window, wc->glContext);
    ImGui_ImplOpenGL3_Init(GL_VERSION);

    ImGuiIO *igio = igGetIO();
    igio->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    bc_EditorEngineContext newEditor = {
        .isActive = isActiveAtStart,
        .showDemoWindow = false,
        .frameTimes = calloc(BC_FRAME_HISTORY_LENGTH, sizeof(float))
    };

    return newEditor;
}

void bc_destroyEditor(bc_EditorEngineContext *eec) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);
}

void bc_resizeEditor(bc_EditorEngineContext *eec) {
    // TODO
}

bool bc_editorWantCaptureMouse(bc_EditorEngineContext *eec) {
    return igGetIO()->WantCaptureMouse;
}

bool bc_editorWantCaptureKeyboard(bc_EditorEngineContext *eec) {
    return igGetIO()->WantCaptureKeyboard;
}

void bc_handleEditorInputEvents(bc_EditorEngineContext *eec, SDL_Event *e) {
    // handle editor toggle
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_BACKQUOTE) {
            eec->isActive ^= 1;
        }
    }
    ImGui_ImplSDL2_ProcessEvent(e);
    // if (eec->isActive) {
    // }
}

void bc_beginEditor() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    igNewFrame();
}

void bc_endEditor() {
    igRender();
    ImDrawData* drawData = igGetDrawData();
    ImGui_ImplOpenGL3_RenderDrawData(drawData);
}

void bc_drawBaseEditor(bc_EditorEngineContext *eec, bc_WindowContext *wc, bc_SuperInfo *superInfo, bc_EngineSettings *engineSettings) {
    // enable docking over entire screen
    igDockSpaceOverViewport(0, igGetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, ImGuiWindowClass_ImGuiWindowClass());

    if (igBegin("Engine Options", NULL, 0)) {
        igText("Press backquote (`/~) to toggle editor");

        igInputInt("Framerate Limit", (int*)&engineSettings->frameRateLimit, 1, 10, 0);
        igInputInt("Tickrate Limit", (int*)&engineSettings->tickRateLimit, 1, 10, 0);
        // TODO: option to save engine settings

        igCheckbox("Demo Window", &eec->showDemoWindow);
        if (eec->showDemoWindow) {
            igShowDemoWindow(&eec->showDemoWindow);
        }
    }
    igEnd();

    if (igBegin("Performance", NULL, 0)) {
        u64 frameIndex = (wc->frame - 1) % BC_FRAME_HISTORY_LENGTH; // Need to get info for previous frame because current frame time hasn't been recorded yet
        // Graph for cpu times
        // Different from true fps, because that may be limited by vsync. Instead, this represents the time it took to prepare a frame, represents max fps if not waiting on the gpu
        {
            float sumOfTimes = 0.0;
            float maxDuration = 0.0;
            for (int i = 0; i < BC_FRAME_HISTORY_LENGTH; i++) {
                float ms = ((float)superInfo->frameCPUTimes[i] / wc->performanceFrequency) * 1000.0;
                eec->frameTimes[i] = ms;
                maxDuration = fmaxf(maxDuration, ms);
                sumOfTimes += superInfo->frameCPUTimes[i];
            }
            float avgTime = ((float)sumOfTimes / BC_FRAME_HISTORY_LENGTH) / wc->performanceFrequency;
            char overlay[32];
            sprintf(overlay, "avg: %.1fms", avgTime * 1000.0);
            igPlotLines_FloatPtr("CPU time (ms)", eec->frameTimes, BC_FRAME_HISTORY_LENGTH, frameIndex, overlay, 0.0, maxDuration, (ImVec2){0, 0}, sizeof(float));

            igValue_Float("Avg potential fps", 1.0 / avgTime, "%.1f");
        }
        // Graph for total frame times
        {
            float sumOfTimes = 0.0;
            float maxDuration = 0.0;
            for (int i = 0; i < BC_FRAME_HISTORY_LENGTH; i++) {
                float ms = ((float)superInfo->frameTotalTimes[i] / wc->performanceFrequency) * 1000.0;
                eec->frameTimes[i] = ms;
                maxDuration = fmaxf(maxDuration, ms);
                sumOfTimes += superInfo->frameTotalTimes[i];
            }
            float avgTime = ((float)sumOfTimes / BC_FRAME_HISTORY_LENGTH) / wc->performanceFrequency;
            char overlay[32];
            sprintf(overlay, "avg: %.1fms", avgTime * 1000.0);
            igPlotLines_FloatPtr("Frame time (ms)", eec->frameTimes, BC_FRAME_HISTORY_LENGTH, frameIndex, overlay, 0.0, maxDuration, (ImVec2){0, 0}, sizeof(float));

            igValue_Float("Avg fps", 1.0 / avgTime, "%.1f");
        }
    }
    igEnd();
}
