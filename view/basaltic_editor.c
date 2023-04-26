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
#include "basaltic_worldState.h"
#include "basaltic_logic.h"
#include "basaltic_commandBuffer.h"
#include "basaltic_components_view.h"
#include "basaltic_components.h"
#include "components/basaltic_components_planes.h"
#include "components/basaltic_components_actors.h"
#include "flecs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

#define MAX_QUERY_EXPR_LENGTH 1024
#define QUERY_PAGE_LENGTH 20

typedef struct {
    ecs_entity_t focusEntity;
    ecs_query_t *query;
    char queryExpr[MAX_QUERY_EXPR_LENGTH];
    char queryExprError[MAX_QUERY_EXPR_LENGTH];
    char worldName[256]; // For providing a unique scope to ImGui fields
} EcsInspectionContext;

typedef struct {
    float *worldStepsPerSecond;

    bool isWorldGenerated;
    u32 worldChunkWidth;
    u32 worldChunkHeight;
    char newGameSeed[BC_MAX_SEED_LENGTH];
} EditorContext;

static EditorContext ec;
static EcsInspectionContext viewInspector;
static EcsInspectionContext modelInspector;


/* ECS General-purpose */
void ecsWorldInspector(ecs_world_t *world, EcsInspectionContext *ic);
void ecsQueryInspector(ecs_world_t *world, EcsInspectionContext *ic);
void ecsEntityInspector(ecs_world_t *world, ecs_entity_t e);
// Returns number of entities in hierarchy, including the root
u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t *focus);

/* Specalized Inspectors */
void modelWorldInspector(bc_WorldState *world, ecs_world_t *viewEcsWorld);
void bitmaskToggle(const char *prefix, u32 *bitmask, u32 toggleBit);
void dateTimeInspector(u64 step);
void coordInspector(const char *label, htw_geo_GridCoord coord);

/* Misc Functions*/
void possessEntity(ecs_world_t *world, ecs_entity_t target);



void bc_setupEditor(void) {
    ec = (EditorContext){
        .isWorldGenerated = false,
        .worldChunkWidth = 3,
        .worldChunkHeight = 3,
    };
    viewInspector = (EcsInspectionContext){.worldName = "View"};
    modelInspector = (EcsInspectionContext){.worldName = "Model"};
}

void bc_teardownEditor(void) {
    // TODO: nothing?
}

// TODO: untangle and replace everything to do with bc_GraphicsState
void bc_drawEditor(bc_SupervisorInterface *si, bc_ModelData *model, bc_CommandBuffer inputBuffer, ecs_world_t *viewWorld, bc_UiState *ui)
{
    /*
    igBegin("Options", NULL, 0);

    const HoveredCell *hc = ecs_singleton_get(viewWorld, HoveredCell);
    igValue_Uint("Hovered cell", hc->cellIndex);
    igValue_Uint("Hovered chunk", hc->chunkIndex);

    igSpacing();

    if (igCollapsingHeader_BoolPtr("Visibility overrides", NULL, 0)) {
        // TODO: custom inspector for ECS enum components
        // if (igButton("Enable All", (ImVec2){0, 0})) {
        //     worldInfo->visibilityOverrideBits = BC_TERRAIN_VISIBILITY_ALL;
        // }
        // if (igButton("Disable All", (ImVec2){0, 0})) {
        //     worldInfo->visibilityOverrideBits = 0;
        // }
        // bitmaskToggle("Geometry", &worldInfo->visibilityOverrideBits, BC_TERRAIN_VISIBILITY_GEOMETRY);
        // bitmaskToggle("Color", &worldInfo->visibilityOverrideBits, BC_TERRAIN_VISIBILITY_COLOR);
    }

    igEnd();
    */

    igBegin("View Inspector", NULL, 0);
    ecsWorldInspector(viewWorld, &viewInspector);
    igEnd();


    igBegin("Model Inspector", NULL, 0);
    if (model == NULL) {
        igText("Generation Settings");

        igSliderInt("(in chunks)##chunkWidth", &ec.worldChunkWidth, 1, 16, "Width: %u", 0);
        igSliderInt("(in chunks)##chunkHeight", &ec.worldChunkHeight, 1, 16, "Height: %u", 0);
        igText("World generation seed:");
        igInputText("##seedInput", ec.newGameSeed, BC_MAX_SEED_LENGTH, 0, NULL, NULL);

        if (igButton("Generate world", (ImVec2){0, 0})) {
            bc_ModelSetupSettings newSetupSettings = {
                .width = ec.worldChunkWidth,
                .height = ec.worldChunkHeight,
                .seed = ec.newGameSeed
            };

            si->modelSettings = newSetupSettings;
            si->signal = BC_SUPERVISOR_SIGNAL_START_MODEL;
        }
    } else {
        bc_WorldState *world = model->world;
        if (igButton("Stop model", (ImVec2){0, 0})) {
            si->signal = BC_SUPERVISOR_SIGNAL_STOP_MODEL;
        }

        /* World info that is safe to inspect at any time: */
        igText("Seed string: %s", world->seedString);

        igValue_Uint("World seed", world->seed);
        igValue_Uint("Logic step", world->step);
        // TODO: convert step to date and time (1 step/hour)
        dateTimeInspector(world->step);

        bc_WorldCommand reusedCommand = {0};

        if (igButton("Advance logic step", (ImVec2){0, 0})) {
            reusedCommand.commandType = BC_COMMAND_TYPE_STEP_ADVANCE;
            bc_pushCommandToBuffer(inputBuffer, &reusedCommand, sizeof(reusedCommand));
        }
        igSameLine(0, -1);
        if (igButton("Start auto step", (ImVec2){0, 0})) {
            reusedCommand.commandType = BC_COMMAND_TYPE_STEP_PLAY;
            bc_pushCommandToBuffer(inputBuffer, &reusedCommand, sizeof(reusedCommand));
        }
        igSameLine(0, -1);
        if (igButton("Stop auto step", (ImVec2){0, 0})) {
            reusedCommand.commandType = BC_COMMAND_TYPE_STEP_PAUSE;
            bc_pushCommandToBuffer(inputBuffer, &reusedCommand, sizeof(reusedCommand));
        }

        igSpacing();

        /* Ensure the model isn't running before doing anything else */
        if (!bc_model_isRunning(model)) {
            if (SDL_SemWaitTimeout(world->lock, 16) != SDL_MUTEX_TIMEDOUT) {
                modelWorldInspector(world, viewWorld);
                ecsWorldInspector(world->ecsWorld, &modelInspector);
                SDL_SemPost(world->lock);
            }
        }
    }
    igEnd();
}

// TODO: will this work with just the model's ecs world?
void modelWorldInspector(bc_WorldState *world, ecs_world_t *viewEcsWorld) {
    // TODO: replace with view focused plane
    ecs_entity_t focusedPlane = ecs_singleton_get(viewEcsWorld, FocusPlane)->entity;
    htw_ChunkMap *cm = ecs_get(world->ecsWorld, focusedPlane, Plane)->chunkMap;

    const HoveredCell *hovered = ecs_singleton_get(viewEcsWorld, HoveredCell);
    const SelectedCell *selected = ecs_singleton_get(viewEcsWorld, SelectedCell);

    if (igCollapsingHeader_TreeNodeFlags("Cell Info", 0)) {
        htw_geo_GridCoord hoveredCellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, hovered->chunkIndex, hovered->cellIndex);
        coordInspector("Hovered cell coordinates", hoveredCellCoord);
        //htw_geo_GridCoord selectedCellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, ui->selectedChunkIndex, ui->selectedCellIndex);
        //coordInspector("Selected cell coordinates", selectedCellCoord);
        igSpacing();

        bc_CellData *hoveredCell = bc_getCellByIndex(cm, hovered->chunkIndex, hovered->cellIndex);
        igText("Cell info:");
        igValue_Int("Height", hoveredCell->height);
        igValue_Int("Temperature", hoveredCell->temperature);
        igValue_Int("Nutrients", hoveredCell->nutrient);
        igValue_Int("Rainfall", hoveredCell->rainfall);
        igValue_Int("Vegetation", hoveredCell->vegetation);
    }

    if (igCollapsingHeader_TreeNodeFlags("Cell Contents", 0)) {
        htw_geo_GridCoord selectedCellCoord = htw_geo_chunkAndCellToGridCoordinates(cm, selected->chunkIndex, selected->cellIndex);

        u32 entityCountHere = 0;
        ecs_entity_t selectedRoot = plane_GetRootEntity(world->ecsWorld, focusedPlane, selectedCellCoord);
        if (selectedRoot != 0) {
            if (ecs_is_valid(world->ecsWorld, selectedRoot)) {
                entityCountHere += hierarchyInspector(world->ecsWorld, selectedRoot, &modelInspector.focusEntity);
            }
        }
        igValue_Uint("Entities here", entityCountHere);
    }

    // Misc options
    if (igButton("Take control of random character", (ImVec2){0, 0})) {
        ecs_iter_t it = ecs_query_iter(world->ecsWorld, world->characters);
        // TODO: not really random!
        ecs_entity_t first = ecs_iter_first(&it);
        possessEntity(world->ecsWorld, first);
    }

    if (igButton("Focus on selected cell", (ImVec2){0, 0})) {
        // TODO: modify this function to take Camera, ChunkMap, and GridCoord
        //bc_focusCameraOnCell(ui, selectedCellCoord);
    }
}

void ecsWorldInspector(ecs_world_t *world, EcsInspectionContext *ic) {
    igPushID_Ptr(world);

    ImVec2 avail;
    igGetContentRegionAvail(&avail);
    if(!igBeginChild_Str("Query", (ImVec2){avail.x * 0.5, 0}, true, 0)) {
        // Early out
        igEndChild();
        return;
    }
    ecsQueryInspector(world, ic);
    igEndChild();

    igSameLine(0, -1);

    if(!igBeginChild_Str("Entity Inspector", (ImVec2){0, 0}, true, 0)) {
        // Early out
        igEndChild();
        return;
    }
    ecsEntityInspector(world, ic->focusEntity);
    igEndChild();

    igPopID();
}

void ecsQueryInspector(ecs_world_t *world, EcsInspectionContext *ic) {

    // TODO: scope-based tree of entities

    // TODO: could evaluate expression after every keypress, but only create query when enter is pressed. Would allow completion hints
    if (igInputText("Query", ic->queryExpr, MAX_QUERY_EXPR_LENGTH, ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL)) {
        ic->queryExprError[0] = '\0';
        if (ic->query != NULL) {
            ecs_query_fini(ic->query);
        }
        ic->query = ecs_query_new(world, ic->queryExpr);
        if (ic->query == NULL) {
            // TODO: figure out if there is a way to get detailed expression error information from Flecs
            strcpy(ic->queryExprError, "Error: invalid query expression");
        }
    }
    if (ic->queryExprError[0] != '\0') {
        igTextColored((ImVec4){1.0, 0.0, 0.0, 1.0}, ic->queryExprError);
    }

    // TODO: Add pages and page navigation. Only show results n..(n+m). Put in a scroll area if list too long
    if (ic->query != NULL) {
        u32 resultsCount = 0;
        ecs_iter_t it = ecs_query_iter(world, ic->query);
        while (ecs_query_next(&it)) {

            for (int i = 0; i < it.count; i++) {
                //resultsCount += hierarchyInspector(world, it.entities[i], ic);
                resultsCount++;
                if (resultsCount < QUERY_PAGE_LENGTH) {
                    // TODO: instead of hierarchy inspector, use query fields to display table of filter term components
                    hierarchyInspector(world, it.entities[i], &ic->focusEntity);
                }
            }
        }
        igValue_Uint("Results", resultsCount);
    }
}

void ecsEntityInspector(ecs_world_t *world, ecs_entity_t e) {
    if(!ecs_is_valid(world, e)) {
        return;
    }
    const char *name = ecs_get_fullpath(world, e);
    igText(name);

    igText("Entity Type: ");
    // NOTE: must be freed by the application
    char* typeStr = ecs_type_str(world, ecs_get_type(world, e));
    igTextWrapped(typeStr);
    ecs_os_free(typeStr);
    // TODO: use reflection info to make inspector for components
}

u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t *focus) {
    u32 count = 1;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    // Highlight focused node
    if (node == *focus) flags |= ImGuiTreeNodeFlags_Selected;

    ecs_iter_t children = ecs_children(world, node);
    // Display as a leaf if no children
    if (!ecs_iter_is_true(&children)) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    bool expandNode = igTreeNodeEx_Str(ecs_get_fullpath(world, node), flags);
    // If tree node was clicked, but not expanded, inspect node
    if (igIsItemClicked(ImGuiMouseButton_Left) && !igIsItemToggledOpen()) {
        *focus = node;
    }

    if (expandNode) {
        // Re-aquire iterator, because ecs_iter_is_true invalidates it
        children = ecs_children(world, node);
        while (ecs_children_next(&children)) {
            for (int i = 0; i < children.count; i++) {
                count += hierarchyInspector(world, children.entities[i], focus);
            }
        }
        igTreePop();
    }
    return count;
}

void possessEntity(ecs_world_t *world, ecs_entity_t target) {
    // TODO: rethink this to work with Ego system
    // Way of doing this that doesn't rely on storing a reference to the 'active character' in uistate: use filters and systems
    // When switching control, first remove PlayerControlled tag from all entities. Then add the tag to target entity
    if (ecs_is_valid(world, target)) {
        ecs_filter_t *pcFilter = ecs_filter(world, {
            .terms = {
                {ecs_pair(Ego, EcsAny)}
            }
        });
        ecs_iter_t fit = ecs_filter_iter(world, pcFilter);
        ecs_defer_begin(world);
        while (ecs_filter_next(&fit)) {
            for (int i = 0; i < fit.count; i++) {
                ecs_remove_pair(world, fit.entities[i], Ego, 0);
            }
        }
        ecs_set_pair(world, target, Ego, 0);
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
