#include <math.h>
#include <stdbool.h>
#include "htw_core.h"
#include "htw_random.h"
#include "basaltic_view.h"
#include "basaltic_editor.h"
#include "basaltic_defs.h"
#include "basaltic_super.h"
#include "basaltic_interaction.h"
#include "basaltic_uiState.h"
#include "basaltic_worldState.h"
#include "basaltic_logic.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_characters.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include"cimgui/cimgui_impl.h"

void bitmaskToggle(const char *prefix, u32 *bitmask, u32 toggleBit);
void characterInspector(bc_Character *character);

bc_EditorContext* bc_view_setupEditor() {
    bc_EditorContext *newEditor = malloc(sizeof(bc_EditorContext));
    *newEditor = (bc_EditorContext){
        .isWorldGenerated = false,
        .worldChunkWidth = 3,
        .worldChunkHeight = 3,
    };

    return newEditor;
}

void bc_view_teardownEditor(bc_EditorContext* ec) {
    free(ec);
}

// TODO: untangle and replace everything to do with bc_GraphicsState
void bc_view_drawEditor(bc_SupervisorInterface *si, bc_EditorContext *ec, bc_ViewContext *vc, bc_ModelData *model, bc_CommandBuffer inputBuffer)
{
    //bc_GraphicsState *graphics = vc->graphics;
    bc_UiState *ui = vc->ui;

    igBegin("Options", NULL, 0);

    igSliderInt("(in chunks)##chunkWidth", &ec->worldChunkWidth, 1, 16, "Width: %u", 0);
    igSliderInt("(in chunks)##chunkHeight", &ec->worldChunkHeight, 1, 16, "Height: %u", 0);
    igText("World generation seed:");
    igInputText("##seedInput", ec->newGameSeed, BC_MAX_SEED_LENGTH, 0, NULL, NULL);

    if (igButton("Generate world", (ImVec2){0, 0})) {
        bc_ModelSetupSettings newSetupSettings = {
            .width = ec->worldChunkWidth,
            .height = ec->worldChunkHeight,
            .seed = ec->newGameSeed
        };

        si->modelSettings = newSetupSettings;
        si->signal = BC_SUPERVISOR_SIGNAL_START_MODEL;
    }

    if (igButton("Stop world model", (ImVec2){0, 0})) {
        si->signal = BC_SUPERVISOR_SIGNAL_STOP_MODEL;
    }

    if (model != NULL) {
        if (model->world != NULL) {
            bc_WorldState *world = model->world;

            igText("Seed string: %s", world->seedString);
            igValue_Uint("Hovered chunk", ui->hoveredChunkIndex);
            igValue_Uint("Hovered cell", ui->hoveredCellIndex);

            igValue_Uint("Mouse x", ui->mouse.x);
            igValue_Uint("Mouse y", ui->mouse.y);

            igSpacing();
            bc_MapChunk hoveredChunk = world->chunks[ui->hoveredChunkIndex];

            igText("Cell info:");
            igValue_Int("Height", htw_geo_getMapValueByIndex(hoveredChunk.heightMap, ui->hoveredCellIndex));
            igValue_Int("Temperature", htw_geo_getMapValueByIndex(hoveredChunk.temperatureMap, ui->hoveredCellIndex));
            igValue_Int("Rainfall", htw_geo_getMapValueByIndex(hoveredChunk.rainfallMap, ui->hoveredCellIndex));
            igSpacing();

            igValue_Uint("World seed", world->seed);
            igValue_Uint("Logic step", world->step);
            // TODO: convert step to date and time (1 step/hour)

            bc_WorldCommand reusedCommand = {0};

            if (igButton("Advance logic step", (ImVec2){0, 0})) {
                reusedCommand.commandType = BC_COMMAND_TYPE_STEP_ADVANCE;
                bc_pushCommandToBuffer(inputBuffer, &reusedCommand, sizeof(reusedCommand));
            }
            if (igButton("Start auto step", (ImVec2){0, 0})) {
                reusedCommand.commandType = BC_COMMAND_TYPE_STEP_PLAY;
                bc_pushCommandToBuffer(inputBuffer, &reusedCommand, sizeof(reusedCommand));
            }
            if (igButton("Stop auto step", (ImVec2){0, 0})) {
                reusedCommand.commandType = BC_COMMAND_TYPE_STEP_PAUSE;
                bc_pushCommandToBuffer(inputBuffer, &reusedCommand, sizeof(reusedCommand));
            }

            //igCheckbox("Draw debug markers", &graphics->showCharacterDebug);

            if (igCollapsingHeader_TreeNodeFlags("Character Inspector", 0)) {
                if (igButton("Take control of random character", (ImVec2){0, 0})) {
                    if (ui->activeCharacter != NULL) {
                        ui->activeCharacter->isControlledByPlayer = false;
                    }
                    u32 randCharacterIndex = htw_randRange(world->characterPoolSize);
                    ui->activeCharacter = &world->characters[randCharacterIndex];
                    ui->activeCharacter->isControlledByPlayer = true;
                    bc_snapCameraToCharacter(ui, ui->activeCharacter);
                }

                if (ui->activeCharacter != NULL) {
                    characterInspector(ui->activeCharacter);
                } else {
                    igText("No active character");
                }
            }

            // if (igCollapsingHeader_BoolPtr("World Info settings", NULL, 0)) {
            //     igSliderInt("Time of day", &graphics->worldInfo.timeOfDay, 0, 255, "%i", 0);
            //     igSliderInt("Sea level", &graphics->worldInfo.seaLevel, 0, 128, "%i", 0);
            // }
            //
            // if (igCollapsingHeader_BoolPtr("Visibility overrides", NULL, 0)) {
            //     if (igButton("Enable All", (ImVec2){0, 0})) {
            //         graphics->worldInfo.visibilityOverrideBits = BC_TERRAIN_VISIBILITY_ALL;
            //     }
            //     if (igButton("Disable All", (ImVec2){0, 0})) {
            //         graphics->worldInfo.visibilityOverrideBits = 0;
            //     }
            //     bitmaskToggle("Geometry", &graphics->worldInfo.visibilityOverrideBits, BC_TERRAIN_VISIBILITY_GEOMETRY);
            //     bitmaskToggle("Color", &graphics->worldInfo.visibilityOverrideBits, BC_TERRAIN_VISIBILITY_COLOR);
            // }
        }
    }

    igEnd();
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
    igValue_Int("Health", character->currentState.currentHitPoints);
    igValue_Int("Stamina", character->currentState.currentStamina);
}