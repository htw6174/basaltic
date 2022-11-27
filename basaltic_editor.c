#include <stdbool.h>
#include "htw_core.h"
#include "htw_vulkan.h"
#include "basaltic_defs.h"
#include "basaltic_editor.h"
#include "basaltic_uiState.h"
#include "basaltic_worldState.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_VULKAN
#define CIMGUI_USE_SDL
#include "cimgui/cimgui.h"
#include"cimgui/cimgui_impl.h"

void bitmaskToggle(const char *prefix, u32 *bitmask, u32 toggleBit);
void characterInspector(u32 characterId);

bt_EditorContext bt_initEditor(htw_VkContext *vkContext) {
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

    bt_EditorContext newEditor = {
        .isActive = true,
        .vkContext = vkContext,
        .showDemoWindow = false,
    };

    return newEditor;
}

void bt_destroyEditor(bt_EditorContext *editorContext) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);
}

void bt_resizeEditor(bt_EditorContext *editorContext) {
    // TODO: is this actually needed? Maybe also set global imgui scale?
    ImGui_ImplVulkan_SetMinImageCount(editorContext->vkContext->swapchainImageCount);
}

bool bt_editorWantCaptureMouse(bt_EditorContext *editorContext) {
    return editorContext->isActive & igGetIO()->WantCaptureMouse;
}

bool bt_editorWantCaptureKeyboard(bt_EditorContext *editorContext) {
    return editorContext->isActive & igGetIO()->WantCaptureMouse;
}

void bt_handleEditorInputEvents(bt_EditorContext *editorContext, SDL_Event *e) {
    // handle editor toggle
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_BACKQUOTE) {
            editorContext->isActive ^= 1;
        }
    }
    if (editorContext->isActive) {
        ImGui_ImplSDL2_ProcessEvent(e);
    }
}

void bt_drawEditor(bt_EditorContext *editorContext, bt_GraphicsState *graphics, bt_UiState *ui, bt_WorldState *world) {
    if (editorContext->isActive) {
        // imgui
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        igNewFrame();

        if (editorContext->showDemoWindow) {
            igShowDemoWindow(&editorContext->showDemoWindow);
        }

        igBegin("Options", NULL, 0);
        igText("Press backquote (`/~) to toggle editor");
        igCheckbox("Demo Window", &editorContext->showDemoWindow);

        igValue_Uint("Hovered chunk: ", ui->hoveredChunkIndex);
        igValue_Uint("Hovered cell: ", ui->hoveredCellIndex);

        igCheckbox("Draw debug markers", &graphics->showCharacterDebug);

        if (igCollapsingHeader_TreeNodeFlags("Character Inspector", 0)) {
            igInputInt("Active character", &ui->activeCharacter, 1, 1, 0);
            characterInspector(ui->activeCharacter);
        }

        if (igCollapsingHeader_BoolPtr("Visibility overrides", NULL, 0)) {
            if (igButton("Enable All", (ImVec2){0, 0})) {
                graphics->worldInfo.visibilityOverrideBits = KD_TERRAIN_VISIBILITY_ALL;
            }
            if (igButton("Disable All", (ImVec2){0, 0})) {
                graphics->worldInfo.visibilityOverrideBits = 0;
            }
            bitmaskToggle("Geometry", &graphics->worldInfo.visibilityOverrideBits, KD_TERRAIN_VISIBILITY_GEOMETRY);
            bitmaskToggle("Color", &graphics->worldInfo.visibilityOverrideBits, KD_TERRAIN_VISIBILITY_COLOR);
        }

        igEnd();

        // end draw
        igRender();
        ImDrawData* drawData = igGetDrawData();
        ImGui_ImplVulkan_RenderDrawData(drawData, editorContext->vkContext->swapchainImages[editorContext->vkContext->currentImageIndex].commandBuffer, NULL);
    }
}

void bitmaskToggle(const char *prefix, u32 *bitmask, u32 toggleBit) {
    bool oldState = ((*bitmask) & toggleBit) > 0;
    bool toggledState = oldState;
    igCheckbox(prefix, &toggledState);

    if (oldState != toggledState) {
        *bitmask = *bitmask ^ toggleBit;
    }
}

void characterInspector(u32 characterId) {
    // TODO
}
