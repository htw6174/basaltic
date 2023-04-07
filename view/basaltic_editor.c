#include <math.h>
#include <stdbool.h>
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "basaltic_view.h"
#include "basaltic_editor.h"
#include "basaltic_defs.h"
#include "basaltic_interaction.h"
#include "basaltic_uiState.h"
#include "basaltic_render.h"
#include "basaltic_worldState.h"
#include "basaltic_logic.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_components.h"
#include "systems/basaltic_character_systems.h"
#include "flecs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

static bc_EditorContext ec;

void worldInspector(bc_UiState *ui, bc_WorldState *world);
void ecsWorldInspector(bc_UiState *ui, bc_WorldState *world);
void possessEntity(ecs_world_t *world, ecs_entity_t target);

void bitmaskToggle(const char *prefix, u32 *bitmask, u32 toggleBit);
void dateTimeInspector(u64 step);
void coordInspector(const char *label, htw_geo_GridCoord coord);
void characterInspector(ecs_world_t *world, ecs_entity_t e);
void entityInspector(ecs_world_t *world, ecs_entity_t e);
// Returns number of entities in hierarchy, including the root
u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node);

void bc_setupEditor(void) {
    ec = (bc_EditorContext){
        .isWorldGenerated = false,
        .worldChunkWidth = 3,
        .worldChunkHeight = 3,
    };
}

void bc_teardownEditor(void) {
    // TODO: nothing?
}

// TODO: untangle and replace everything to do with bc_GraphicsState
void bc_drawEditor(bc_SupervisorInterface *si, bc_ModelData *model, bc_CommandBuffer inputBuffer, bc_RenderContext *rc, bc_UiState *ui)
{
    igBegin("Options", NULL, 0);

    igSliderInt("(in chunks)##chunkWidth", &ec.worldChunkWidth, 1, 16, "Width: %u", 0);
    igSliderInt("(in chunks)##chunkHeight", &ec.worldChunkHeight, 1, 16, "Height: %u", 0);
    igText("World generation seed:");
    igInputText("##seedInput", ec.newGameSeed, BC_MAX_SEED_LENGTH, 0, NULL, NULL);

    igText("Camera Position");
    igValue_Float("x", ui->cameraX, "%.3f");
    igSameLine(0, -1);
    igValue_Float("y", ui->cameraY, "%.3f");
    igValue_Float("Camera Distance", ui->cameraDistance, "%.3f");
    igValue_Float("Camera Pitch", ui->cameraPitch, "%.3f");
    igValue_Float("Camera Yaw", ui->cameraYaw, "%.3f");

    if (igButton("Generate world", (ImVec2){0, 0})) {
        bc_ModelSetupSettings newSetupSettings = {
            .width = ec.worldChunkWidth,
            .height = ec.worldChunkHeight,
            .seed = ec.newGameSeed
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

            /* World info that is safe to inspect at any time: */
            igText("Seed string: %s", world->seedString);

            igValue_Uint("World seed", world->seed);
            igValue_Uint("Logic step", world->step);
            // TODO: convert step to date and time (1 step/hour)
            dateTimeInspector(world->step);
            igSpacing();

            if(igSliderInt("(in half chunks)##renderDistance", &rc->chunkVisibilityRadius, 1, 16, "View Distance: %u", 0)) {
                rc->windowInfo.visibilityRadius = rc->chunkVisibilityRadius * 32.0;
            }
            igInputFloat("Fog Extinction", &rc->windowInfo.fogExtinction, 0.001, 0.01, "%f", 0);
            igInputFloat("Fog Inscattering", &rc->windowInfo.fogInscattering, 0.0001, 0.001, "%f", 0);

            coordInspector("Mouse", (htw_geo_GridCoord){ui->mouse.x, ui->mouse.y});
            igValue_Uint("Hovered chunk", ui->hoveredChunkIndex);
            igValue_Uint("Hovered cell", ui->hoveredCellIndex);
            igSpacing();

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
            igSpacing();

            igCheckbox("Draw entity markers", &rc->drawSystems);

            /* Ensure the model isn't running before doing anything else */
            if (!bc_model_isRunning(model)) {
                if (SDL_SemWaitTimeout(world->lock, 16) != SDL_MUTEX_TIMEDOUT) {
                    worldInspector(ui, world);
                    SDL_SemPost(world->lock);
                }
            }
        }
    }

    bc_WorldInfo *worldInfo = &rc->worldInfo;
    if (igCollapsingHeader_BoolPtr("World Info settings", NULL, 0)) {
        igSliderInt("Time of day", &worldInfo->timeOfDay, 0, 255, "%i", 0);
        igSliderInt("Sea level", &worldInfo->seaLevel, 0, 128, "%i", 0);
    }

    if (igCollapsingHeader_BoolPtr("Visibility overrides", NULL, 0)) {
        if (igButton("Enable All", (ImVec2){0, 0})) {
            worldInfo->visibilityOverrideBits = BC_TERRAIN_VISIBILITY_ALL;
        }
        if (igButton("Disable All", (ImVec2){0, 0})) {
            worldInfo->visibilityOverrideBits = 0;
        }
        bitmaskToggle("Geometry", &worldInfo->visibilityOverrideBits, BC_TERRAIN_VISIBILITY_GEOMETRY);
        bitmaskToggle("Color", &worldInfo->visibilityOverrideBits, BC_TERRAIN_VISIBILITY_COLOR);
    }

    igEnd();
}

void worldInspector(bc_UiState *ui, bc_WorldState *world) {
    const bc_TerrainMap *tm = ecs_get(world->ecsWorld, ui->focusedTerrain, bc_TerrainMap);

    if (igCollapsingHeader_TreeNodeFlags("Cell Info", 0)) {
        htw_geo_GridCoord hoveredCellCoord = htw_geo_chunkAndCellToGridCoordinates(tm->chunkMap, ui->hoveredChunkIndex, ui->hoveredCellIndex);
        coordInspector("Hovered cell coordinates", hoveredCellCoord);
        htw_geo_GridCoord selectedCellCoord = htw_geo_chunkAndCellToGridCoordinates(tm->chunkMap, ui->selectedChunkIndex, ui->selectedCellIndex);
        coordInspector("Selected cell coordinates", selectedCellCoord);
        igSpacing();

        bc_CellData *hoveredCell = bc_getCellByIndex(tm->chunkMap, ui->hoveredChunkIndex, ui->hoveredCellIndex);
        igText("Cell info:");
        igValue_Int("Height", hoveredCell->height);
        igValue_Int("Temperature", hoveredCell->temperature);
        igValue_Int("Nutrients", hoveredCell->nutrient);
        igValue_Int("Rainfall", hoveredCell->rainfall);
        igValue_Int("Vegetation", hoveredCell->vegetation);
    }

    //igCheckbox("Draw debug markers", &graphics->showCharacterDebug);
    ecsWorldInspector(ui, world);
}

void ecsWorldInspector(bc_UiState *ui, bc_WorldState *world) {
    const bc_TerrainMap *tm = ecs_get(world->ecsWorld, ui->focusedTerrain, bc_TerrainMap);
    htw_geo_GridCoord selectedCellCoord = htw_geo_chunkAndCellToGridCoordinates(tm->chunkMap, ui->selectedChunkIndex, ui->selectedCellIndex);
    if (igCollapsingHeader_TreeNodeFlags("Character Inspector", 0)) {

        // In fact, this should be true for non-editor tasks as well, as long as they only touch the ECS. Should make implementing player actions much simpler.
        static int spawnCount = 100;
        igInputInt("Spawn Count", &spawnCount, 1, 100, 0);
        if (igButton("Spawn grazers", (ImVec2){0, 0})) {
            bc_createCharacters(world->ecsWorld, ui->focusedTerrain, spawnCount);
        }

        if (igButton("Take control of random character", (ImVec2){0, 0})) {
            ecs_iter_t it = ecs_query_iter(world->ecsWorld, world->characters);
            // TODO: not really random!
            ecs_entity_t first = ecs_iter_first(&it);
            possessEntity(world->ecsWorld, first);
        }

        u32 entityCountHere = 0;
        khint_t khi = kh_get(GridMap, tm->gridMap, selectedCellCoord);
        if (khi != kh_end(tm->gridMap)) {
            ecs_entity_t root = kh_val(tm->gridMap, khi);
            if (ecs_is_valid(world->ecsWorld, root)) {
                entityCountHere += hierarchyInspector(world->ecsWorld, root);
            }
        }
        igValue_Uint("Entities here", entityCountHere);
        if (igButton("Focus on selected cell", (ImVec2){0, 0})) {
            bc_focusCameraOnCell(ui, selectedCellCoord);
        }

        // TODO: filter for showing all player controlled characters
    }
}

void possessEntity(ecs_world_t *world, ecs_entity_t target) {
    // Way of doing this that doesn't rely on storing a reference to the 'active character' in uistate: use filters and systems
    // When switching control, first remove PlayerControlled tag from all entities. Then add the tag to target entity
    if (ecs_is_valid(world, target)) {
        ecs_filter_t *pcFilter = ecs_filter(world, {
            .terms = {
                {PlayerControlled}
            }
        });
        ecs_iter_t fit = ecs_filter_iter(world, pcFilter);
        ecs_defer_begin(world);
        while (ecs_filter_next(&fit)) {
            for (int i = 0; i < fit.count; i++) {
                ecs_remove(world, fit.entities[i], PlayerControlled);
            }
        }
        ecs_add(world, target, PlayerControlled);
        ecs_add(world, target, PlayerVision);
        ecs_defer_end(world);

        // TODO: do this outside possessEntity
        //htw_geo_GridCoord coord = *ecs_get(world, target, bc_GridPosition);
        //bc_focusCameraOnCell(ui, coord);
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

void dateTimeInspector(u64 step) {
    u64 year = step / (24 * 30 * 12);
    u64 month = ((step / (24 * 30)) % 12) + 1;
    u64 day = ((step / 24) % 30) + 1;
    u64 hour = step % 24;

    igText("%i/%i/%i Hour %i", year, month, day, hour);
}

void coordInspector(const char *label, htw_geo_GridCoord coord) {
    igText(label);
    igValue_Int("X", coord.x);
    igSameLine(igGetCursorPosX() + 64.0, -1);
    igValue_Int("Y", coord.y);
}

void characterInspector(ecs_world_t *world, ecs_entity_t e) {
    igValue_Uint("Entity ID", e);
    igSameLine(0, -1);
    if (igButton("Take control", (ImVec2){0, 0})) {
        possessEntity(world, e);
    }
    igText("Entity Type: "); igSameLine(0, -1);
    // TODO: need to free this after use? Try freeing immediately, see if there are any issues
    char* typeStr = ecs_type_str(world, ecs_get_type(world, e));
    igTextWrapped(typeStr);
    ecs_os_free(typeStr);
    //igValue_Int("Health", character->currentState.currentHitPoints);
    if (ecs_has(world, e, bc_Stamina)) {
        igValue_Int("Stamina", ecs_get(world, e, bc_Stamina)->currentStamina);
    }
    if (ecs_has(world, e, bc_GridPosition)) {
        coordInspector("Current Position", *ecs_get(world, e, bc_GridPosition));
    }
    if (ecs_has(world, e, bc_GridDestination)) {
        coordInspector("Target Position", *ecs_get(world, e, bc_GridDestination));
    }
}

void entityInspector(ecs_world_t *world, ecs_entity_t e) {
    igValue_Uint("Entity ID", e);
    igSameLine(0, -1);
    if (igButton("Take control", (ImVec2){0, 0})) {
        possessEntity(world, e);
    }
    igText("Entity Type: ");
    // TODO: need to free this after use? Try freeing immediately, see if there are any issues
    char* typeStr = ecs_type_str(world, ecs_get_type(world, e));
    igTextWrapped(typeStr);
    ecs_os_free(typeStr);
}

u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node) {
    u32 count = 1;
    if (igTreeNodeEx_Str(ecs_get_fullpath(world, node), ImGuiTreeNodeFlags_DefaultOpen)) {
        entityInspector(world, node);
        ecs_iter_t children = ecs_children(world, node);
        while (ecs_children_next(&children)) {
            for (int i = 0; i < children.count; i++) {
                count += hierarchyInspector(world, children.entities[i]);
            }
        }
        igTreePop();
    }
    return count;
}
