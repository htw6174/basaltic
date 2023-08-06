
#include <stdbool.h>
#include "htw_core.h"
#include "basaltic_editor_base.h"
#include "basaltic_super.h"
#include "basaltic_view.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_OPENGL3
#define CIMGUI_USE_SDL2
#include "cimgui/cimgui.h"
#include "cimgui/cimgui_impl.h"

bc_EditorEngineContext bc_initEditor(bool isActiveAtStart, bc_WindowContext *wc) {
    // TODO: the organization here is awkward; this module doesn't need to know about vulkan specifics or content of the vkContext struct, except to do the cimgui setup here. Putting this in htw_vulkan would require that library to also be aware of cimgui. Not sure of the best way to resolve this
    // TODO: imgui saves imgui.ini in the cwd by default, which will usually be the data folder for this project. Consider changing it to a more useful default by setting io.IniFileName
    igCreateContext(NULL);
    ImGui_ImplSDL2_InitForOpenGL(wc->window, wc->glContext);
    ImGui_ImplOpenGL3_Init("#version 330");

    ImGuiIO *igio = igGetIO();
    igio->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    bc_EditorEngineContext newEditor = {
        .isActive = isActiveAtStart,
        .showDemoWindow = false,
        .frameTimeSeconds = calloc(bc_frameHistoryLength, sizeof(float))
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
    return eec->isActive & igGetIO()->WantCaptureMouse;
}

bool bc_editorWantCaptureKeyboard(bc_EditorEngineContext *eec) {
    return eec->isActive & igGetIO()->WantCaptureKeyboard;
}

void bc_handleEditorInputEvents(bc_EditorEngineContext *eec, SDL_Event *e) {
    // handle editor toggle
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_BACKQUOTE) {
            eec->isActive ^= 1;
        }
    }
    if (eec->isActive) {
        ImGui_ImplSDL2_ProcessEvent(e);
    }
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
    igDockSpaceOverViewport(igGetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, ImGuiWindowClass_ImGuiWindowClass());

    if (igBegin("Engine Options", NULL, 0)) {
        igText("Press backquote (`/~) to toggle editor");

        igInputInt("Framerate Limit", &engineSettings->frameRateLimit, 1, 10, 0);
        igInputInt("Tickrate Limit", &engineSettings->tickRateLimit, 1, 10, 0);
        // TODO: option to save engine settings

        igCheckbox("Demo Window", &eec->showDemoWindow);
        if (eec->showDemoWindow) {
            igShowDemoWindow(&eec->showDemoWindow);
        }
    }
    igEnd();

    if (igBegin("Performance", NULL, 0)) {
        u64 frameMod = (wc->frame - 1) % bc_frameHistoryLength; // Need to get info for previous frame because current frame time hasn't been recorded yet

        float sumOfTimes = 0.0;
        float maxDuration = 0.0;
        for (int i = 0; i < bc_frameHistoryLength; i++) {
            float secs = (float)superInfo->frameDurations[i] / wc->performanceFrequency;
            eec->frameTimeSeconds[i] = secs;
            maxDuration = fmaxf(maxDuration, secs);
            sumOfTimes += superInfo->frameDurations[i];
        }
        igPlotLines_FloatPtr("Frame time", eec->frameTimeSeconds, bc_frameHistoryLength, frameMod, "", 0.0, maxDuration, (ImVec2){0, 0}, sizeof(float));

        // Not calculating 'real' fps, because that may be limited by vsync. Instead, this represents the time it took to prepare a frame, and is a better indication of potential max fps
        float avgFrameTime = ((float)sumOfTimes / bc_frameHistoryLength) / wc->performanceFrequency;
        igValue_Float("Avg potential fps", 1.0 / avgFrameTime, "%.1f");
    }
    igEnd();
}
