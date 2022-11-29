#include <stdbool.h>
#include "htw_core.h"
#include "htw_random.h"
#include "htw_vulkan.h"
#include "basaltic_defs.h"
#include "basaltic_editor.h"
#include "basaltic_uiState.h"
#include "basaltic_worldState.h"
#include "basaltic_characters.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_VULKAN
#define CIMGUI_USE_SDL
#include "cimgui/cimgui.h"
#include"cimgui/cimgui_impl.h"

void bitmaskToggle(const char *prefix, u32 *bitmask, u32 toggleBit);
void characterInspector(bc_Character *character);

bc_EditorContext bc_initEditor(htw_VkContext *vkContext) {
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

    bc_EditorContext newEditor = {
        .isActive = true,
        .vkContext = vkContext,
        .showDemoWindow = false,
    };

    return newEditor;
}

void bc_destroyEditor(bc_EditorContext *editorContext) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);
}

void bc_resizeEditor(bc_EditorContext *editorContext) {
    // TODO: is this actually needed? Maybe also set global imgui scale?
    ImGui_ImplVulkan_SetMinImageCount(editorContext->vkContext->swapchainImageCount);
}

bool bc_editorWantCaptureMouse(bc_EditorContext *editorContext) {
    return editorContext->isActive & igGetIO()->WantCaptureMouse;
}

bool bc_editorWantCaptureKeyboard(bc_EditorContext *editorContext) {
    return editorContext->isActive & igGetIO()->WantCaptureMouse;
}

void bc_handleEditorInputEvents(bc_EditorContext *editorContext, SDL_Event *e) {
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

void bc_drawEditor(bc_EditorContext *editorContext, bc_GraphicsState *graphics, bc_UiState *ui, bc_WorldState *world) {
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

        if (ui->interfaceMode == BC_INTERFACE_MODE_GAMEPLAY) {
            igValue_Uint("Hovered chunk: ", ui->hoveredChunkIndex);
            igValue_Uint("Hovered cell: ", ui->hoveredCellIndex);

            igCheckbox("Draw debug markers", &graphics->showCharacterDebug);

            if (igCollapsingHeader_TreeNodeFlags("Character Inspector", 0)) {
                if (igButton("Take control of random character", (ImVec2){0, 0})) {
                    u32 randCharacterIndex = htw_randRange(world->characterPoolSize);
                    ui->activeCharacter = &world->characters[randCharacterIndex];
                }

                if (ui->activeCharacter != NULL) {
                    characterInspector(ui->activeCharacter);
                } else {
                    igText("No active character");
                }
            }

            if (igCollapsingHeader_BoolPtr("Visibility overrides", NULL, 0)) {
                if (igButton("Enable All", (ImVec2){0, 0})) {
                    graphics->worldInfo.visibilityOverrideBits = BC_TERRAIN_VISIBILITY_ALL;
                }
                if (igButton("Disable All", (ImVec2){0, 0})) {
                    graphics->worldInfo.visibilityOverrideBits = 0;
                }
                bitmaskToggle("Geometry", &graphics->worldInfo.visibilityOverrideBits, BC_TERRAIN_VISIBILITY_GEOMETRY);
                bitmaskToggle("Color", &graphics->worldInfo.visibilityOverrideBits, BC_TERRAIN_VISIBILITY_COLOR);
            }
        } else {
            if (igButton("Start new game", (ImVec2){0, 0})) {
                ui->interfaceMode = BC_INTERFACE_MODE_GAMEPLAY;
            }
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

void characterInspector(bc_Character *character) {
    // TODO
    igValue_Uint("Character ID", character->id);
}
