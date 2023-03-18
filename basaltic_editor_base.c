
#include <stdbool.h>
#include "htw_core.h"
#include "htw_vulkan.h"
#include "basaltic_editor_base.h"
#include "basaltic_super.h"
#include "basaltic_view.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_VULKAN
#define CIMGUI_USE_SDL
#include "cimgui/cimgui.h"
#include"cimgui/cimgui_impl.h"

bc_EditorEngineContext bc_initEditor(bool isActiveAtStart, htw_VkContext *vkContext) {
    // TODO: the organization here is awkward; this module doesn't need to know about vulkan specifics or content of the vkContext struct, except to do the cimgui setup here. Putting this in htw_vulkan would require that library to also be aware of cimgui. Not sure of the best way to resolve this
    // TODO: imgui saves imgui.ini in the cwd by default, which will usually be the data folder for this project. Consider changing it to a more useful default by setting io.IniFileName
    igCreateContext(NULL);
    ImGui_ImplSDL2_InitForVulkan(vkContext->window);
    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = vkContext->instance,
        .PhysicalDevice = vkContext->gpu,
        .Device = vkContext->device,
        .QueueFamily = vkContext->graphicsQueueIndex,
        .Queue = vkContext->queue,
        .PipelineCache = VK_NULL_HANDLE,
        .DescriptorPool = vkContext->descriptorPool,
        .Subpass = 0,
        .MinImageCount = 2,
        .ImageCount = vkContext->swapchainImageCount,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .Allocator = NULL,
        .CheckVkResultFn = NULL
    };
    ImGui_ImplVulkan_Init(&init_info, vkContext->renderPass);

    htw_beginOneTimeCommands(vkContext);
    ImGui_ImplVulkan_CreateFontsTexture(vkContext->oneTimeBuffer);
    htw_endOneTimeCommands(vkContext);
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    bc_EditorEngineContext newEditor = {
        .isActive = isActiveAtStart,
        .vkContext = vkContext,
        .showDemoWindow = false,
        .maxFrameDuration = 0,
        .maxStepsPerSecond = 0,
        .frameDurationHistory = calloc(bc_frameHistoryLength, sizeof(float)),
        .tickDurationHistory = calloc(bc_frameHistoryLength, sizeof(float)),
    };

    return newEditor;
}

void bc_destroyEditor(bc_EditorEngineContext *eec) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);
}

void bc_resizeEditor(bc_EditorEngineContext *eec) {
    // TODO: is this actually needed? Maybe also set global imgui scale?
    ImGui_ImplVulkan_SetMinImageCount(eec->vkContext->swapchainImageCount);
}

bool bc_editorWantCaptureMouse(bc_EditorEngineContext *eec) {
    return eec->isActive & igGetIO()->WantCaptureMouse;
}

bool bc_editorWantCaptureKeyboard(bc_EditorEngineContext *eec) {
    return eec->isActive & igGetIO()->WantCaptureMouse;
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
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    igNewFrame();
}

void bc_endEditor(bc_EditorEngineContext *eec) {
    igRender();
    ImDrawData* drawData = igGetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, eec->vkContext->swapchainImages[eec->vkContext->currentImageIndex].commandBuffer, NULL);
}

void bc_drawEditor(bc_EditorEngineContext *eec, bc_WindowContext *wc, bc_SuperInfo *superInfo, bc_EngineSettings *engineSettings) {
    igBegin("Engine Options", NULL, 0);

    igText("Press backquote (`/~) to toggle editor");

    igInputInt("Framerate Limit", &engineSettings->frameRateLimit, 1, 10, 0);
    igInputInt("Tickrate Limit", &engineSettings->tickRateLimit, 1, 10, 0);
    // TODO: option to save engine settings

    igCheckbox("Demo Window", &eec->showDemoWindow);
    if (eec->showDemoWindow) {
        igShowDemoWindow(&eec->showDemoWindow);
    }

    // NOTE: model start/stop buttons have been moved to the Model module. Any reason to restore the functionality here?
    // if (igButton("Start Model", (ImVec2){0, 0})) {
    //     bc_requestModelStop();
    //     eec->modelRestarting = true;
    // }
    //
    // if (!eec->modelRestarting) {
    //     if (igButton("Stop Model", (ImVec2){0, 0})) {
    //         bc_requestModelStop();
    //     }
    // }
    //
    // if (eec->modelRestarting == true) {
    //     if (bc_isModelRunning() == false) {
    //         // TODO: maybe shouldn't do this here. Parameters need to be added to ModelStartSettings struct
    //         //bc_startNewGame(eec->worldChunkWidth, eec->worldChunkHeight, eec->newGameSeed);
    //         eec->modelRestarting = false;
    //     }
    // }

    igEnd();

    igBegin("Performance", NULL, 0);

    u64 frameMod = (wc->frame - 1) % bc_frameHistoryLength; // Need to get info for previous frame because current frame time hasn't been recorded yet
    float lastFrameDuration = (float)superInfo->frameDurations[frameMod] / 1000.0;
    eec->maxFrameDuration = fmaxf(eec->maxFrameDuration, lastFrameDuration);
    eec->frameDurationHistory[frameMod] = lastFrameDuration;
    igPlotLines_FloatPtr("Frame time", eec->frameDurationHistory, bc_frameHistoryLength, frameMod, "", 0.0, eec->maxFrameDuration, (ImVec2){0, 0}, sizeof(float));

    // u64 lastWorldStep = superInfo->worldStepHistory[frameMod];
    // u64 previousSecondFrameMod = (wc->frame - 61) % bc_frameHistoryLength;
    // u64 previousSecondWorldStep = superInfo->worldStepHistory[previousSecondFrameMod];
    // float stepsInLastSecond = (float)(lastWorldStep - previousSecondWorldStep);
    // eec->maxStepsPerSecond = fmaxf(eec->maxStepsPerSecond, stepsInLastSecond);
    // eec->worldStepsPerSecond[frameMod] = stepsInLastSecond;
    // igPlotLines_FloatPtr("Steps per second", eec->worldStepsPerSecond, bc_frameHistoryLength, frameMod, "", 0.0, eec->maxStepsPerSecond, (ImVec2){0, 0}, sizeof(float));

    igEnd();
}
