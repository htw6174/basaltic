#include <math.h>
#include <stdbool.h>
#include <float.h>
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
#include "flecs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

#define MAX_CUSTOM_QUERIES 4
#define MAX_QUERY_EXPR_LENGTH 1024

typedef struct {
    ecs_query_t *query;
    s32 queryPage;
    s32 queryPageLength;
    char queryExpr[MAX_QUERY_EXPR_LENGTH];
    char queryExprError[MAX_QUERY_EXPR_LENGTH];
} QueryContext;

typedef struct {
    ecs_entity_t focusEntity;
    // TODO: add focus history, change focus by writing to / cycling through history. Add back/forward buttons or recently viewed list
    ecs_query_t *components;
    ecs_query_t *tags;
    ecs_query_t *relationships;
    // user defined queries
    QueryContext customQueries[MAX_CUSTOM_QUERIES];
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
void ecsQueryColumns(ecs_world_t *world, EcsInspectionContext *ic);
void ecsQueryInspector(ecs_world_t *world, QueryContext *qc, ecs_entity_t *selected);
void ecsTreeInspector(ecs_world_t *world, ecs_entity_t *selected);
void ecsEntityInspector(ecs_world_t *world, EcsInspectionContext *ic);
void entityCreator(ecs_world_t *world, ecs_entity_t parent, EcsInspectionContext *ic);
/** Displays an igInputText for [name]]. Returns true when [name] is set to a unique name in parent scope and submitted. When used within a popup, auto-focuses input field and closes popup when confirming. */
bool entityRenamer(ecs_world_t *world, ecs_entity_t parent, char *name, size_t name_buf_size);
/** Returns number of entities in hierarchy, including the root */
u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t *focus, bool defaultOpen);
bool entitySelector(ecs_world_t *world, ecs_query_t *query, ecs_entity_t *selected);
bool pairSelector(ecs_world_t *world, ecs_query_t *relationshipQuery, ecs_id_t *selected);
/** Displays query results as a scrollable list of igSelectable. If filter is not NULL, filters list by entity name. Sets [selected] to id of selected item. Returns true when an item is clicked */
bool entityList(ecs_world_t *world, ecs_iter_t *iter, ImGuiTextFilter *filter, ImVec2 size, s32 maxDisplayedResults, ecs_entity_t *selected);
void componentInspector(ecs_world_t *world, ecs_entity_t e, ecs_entity_t component, ecs_entity_t *focusEntity);
void ecsMetaInspector(ecs_world_t *world, ecs_meta_cursor_t *cursor, ecs_entity_t *focusEntity);
/** If kind is EcsEntity and focus entity is not NULL, will add a button to set focusEntity to component value */
void primitiveInspector(ecs_world_t *world, ecs_primitive_kind_t kind, void *field, ecs_entity_t *focusEntity);
/** Return entity name if it is named, otherwise return stringified ID (ID string is shared between calls, use immediately or copy to preserve) */
const char *getEntityLabel(ecs_world_t *world, ecs_entity_t e);
/** Display entity name if it is named, otherwise display id */
void entityLabel(ecs_world_t *world, ecs_entity_t e);

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
    viewInspector = (EcsInspectionContext){
        .worldName = "View",
        .customQueries = {
            [0] = {.queryExpr = "Camera"},
            [1] = {.queryExpr = "Pipeline || InstanceBuffer"}, //(bcview.RenderPipeline, _)"},
            [2] = {.queryExpr = "Prefab"},
            [3] = {.queryExpr = "flecs.system.System"},
        }
    };

    modelInspector = (EcsInspectionContext){0};
}

void bc_teardownEditor(void) {
    // TODO: nothing?
}

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
                // set model ecs world scope, to keep view's external tags/queries separate
                ecs_entity_t oldScope = ecs_get_scope(world->ecsWorld);
                ecs_entity_t viewScope = ecs_entity_init(world->ecsWorld, &(ecs_entity_desc_t){.name = "bcview"});
                ecs_set_scope(world->ecsWorld, viewScope);
                modelWorldInspector(world, viewWorld);
                ecsWorldInspector(world->ecsWorld, &modelInspector);
                ecs_set_scope(world->ecsWorld, oldScope);
                SDL_SemPost(world->lock);
            }
        }
    }
    igEnd();
}

void bc_editorOnModelStart(void) {
    modelInspector = (EcsInspectionContext){
        .worldName = "Model",
        .customQueries = {
            [0] = {.queryExpr = "Position"},
            [1] = {.queryExpr = "bc.planes.Plane"},
            [2] = {.queryExpr = "Prefab"},
            [3] = {.queryExpr = "flecs.system.System"},
        }
    };
}

void bc_editorOnModelStop(void) {
    modelInspector = (EcsInspectionContext){0};
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
                entityCountHere += hierarchyInspector(world->ecsWorld, selectedRoot, &modelInspector.focusEntity, true);
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

    // Menu bar for common actions
    if (igButton("New Entity", (ImVec2){0, 0})) {
        igOpenPopup_Str("create_entity", 0);
    }
    if (igBeginPopup("create_entity", 0)) {
        entityCreator(world, 0, ic);
        igEndPopup();
    }

    ImVec2 avail;
    igGetContentRegionAvail(&avail);
    // Top third: tabs or columns for custom queries
    if(igBeginChild_Str("Queries", (ImVec2){0, avail.y * 0.4}, true, ImGuiWindowFlags_MenuBar)) {
        if (igBeginMenuBar()) {
            igText("Custom Queries");
            igEndMenuBar();
        }
        ecsQueryColumns(world, ic);
    }
    igEndChild();

    // Bottom-left third: Tree view of world
    if(igBeginChild_Str("Tree", (ImVec2){avail.x * 0.5, 0}, true, ImGuiWindowFlags_MenuBar)) {
        if (igBeginMenuBar()) {
            igText("World Tree");
            igEndMenuBar();
        }
        ecsTreeInspector(world, &ic->focusEntity);
    }
    igEndChild();

    igSameLine(0, -1);

    // Bottom-right third: entity inspector
    if(igBeginChild_Str("Entity Inspector", (ImVec2){0, 0}, true, ImGuiWindowFlags_MenuBar)) {
        if (igBeginMenuBar()) {
            igText("Entity Inspector");
            igEndMenuBar();
        }
        ecsEntityInspector(world, ic);
    }
    igEndChild();

    igPopID();
}

void ecsQueryColumns(ecs_world_t *world, EcsInspectionContext *ic) {
    ImVec2 avail;
    igGetContentRegionAvail(&avail);
    for (int i = 0; i < MAX_CUSTOM_QUERIES; i++) {
        igPushID_Int(i);
        if(igBeginChild_Str("Custom Query", (ImVec2){avail.x / MAX_CUSTOM_QUERIES, 0}, true, 0)) {
            ecsQueryInspector(world, &ic->customQueries[i], &ic->focusEntity);
        }
        igEndChild();
        igPopID();
        igSameLine(0, -1);
    }
}

void ecsQueryInspector(ecs_world_t *world, QueryContext *qc, ecs_entity_t *selected) {
    // TODO: could evaluate expression after every keypress, but only create query when enter is pressed. Would allow completion hints
    // NOTE: Flecs explorer's entity tree also includes these terms in every query, to alter entity display by property (last term means singleton): "?Module, ?Component, ?Tag, ?Prefab, ?Disabled, ?ChildOf(_, $This)"
    if (igIsWindowAppearing() || igInputText("Query", qc->queryExpr, MAX_QUERY_EXPR_LENGTH, ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL)) {
        qc->queryExprError[0] = '\0';
        if (qc->query != NULL) {
            ecs_query_fini(qc->query);
        }
        qc->query = ecs_query_new(world, qc->queryExpr);
        if (qc->query == NULL) {
            // TODO: figure out if there is a way to get detailed expression error information from Flecs
            strcpy(qc->queryExprError, "Error: invalid query expression");
        }
        qc->queryPage = 0;
    }
    if (qc->queryExprError[0] != '\0') {
        igTextColored((ImVec4){1.0, 0.0, 0.0, 1.0}, qc->queryExprError);
    }
    igInputInt("Results per page", &qc->queryPageLength, 1, 1, 0);
    qc->queryPageLength = max_int(qc->queryPageLength, 10);

    // TODO: Put in a scroll area if list too long
    if (qc->query != NULL) {
        igPushID_Str("Query Results");
        u32 resultsCount = 0;
        u32 minDisplayed = qc->queryPageLength * qc->queryPage;
        u32 maxDisplayed = minDisplayed + qc->queryPageLength;
        ecs_iter_t it = ecs_query_iter(world, qc->query);
        while (ecs_query_next(&it)) {
            for (int i = 0; i < it.count; i++) {
                //resultsCount += hierarchyInspector(world, it.entities[i], qc);
                if (resultsCount >= minDisplayed && resultsCount < maxDisplayed) {
                    ecs_entity_t e = it.entities[i];
                    const char *name = getEntityLabel(world, e);
                    igPushID_Int(e);
                    // NOTE: igSelectable will, by default, call CloseCurrentPopup when clicked. Set flag to disable this behavior
                    if (igSelectable_Bool(name, e == *selected, ImGuiSelectableFlags_DontClosePopups, (ImVec2){0, 0})) {
                        *selected = e;
                    }
                    // TODO: should be its own function
                    if (igBeginPopupContextItem("##context_menu", ImGuiPopupFlags_MouseButtonRight)) {
                        if (igSelectable_Bool("Create Instance", false, 0, (ImVec2){0, 0})) {
                            ecs_new_w_pair(world, EcsIsA, e);
                        }
                        // TODO: more context options
                        igEndPopup();
                    }
                    igPopID();
                }
                resultsCount++;
            }
        }
        // TODO: after converting to table, add blank rows so other controls don't change position
        igValue_Uint("Results", resultsCount);
        s32 pageCount = (s32)ceilf((float)resultsCount / qc->queryPageLength) - 1;
        if (pageCount > 0) {
            if (igArrowButton("page_left", ImGuiDir_Left)) qc->queryPage--;
            igSameLine(0, -1);
            if (igArrowButton("page_right", ImGuiDir_Right)) qc->queryPage++;
            igSameLine(0, -1);
            igSliderInt("Page", &qc->queryPage, 0, pageCount, NULL, ImGuiSliderFlags_AlwaysClamp);
        }
        qc->queryPage = min_int(qc->queryPage, pageCount);
        igPopID();
    }
}

// TODO: should limit max number of displayed entities. For now, make sure bulk creation happens in some scope
void ecsTreeInspector(ecs_world_t *world, ecs_entity_t *selected) {
    ecs_iter_t it = ecs_children(world, 0);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            hierarchyInspector(world, it.entities[i], selected, false);
        }
    }
}

void ecsEntityInspector(ecs_world_t *world, EcsInspectionContext *ic) {
    ecs_entity_t e = ic->focusEntity;
    if(!ecs_is_valid(world, e)) {
        return;
    }
    // TODO: shouldn't let any entity be deleted and/or disabled. Components, anything in the flecs namespace; systems should only be disabled, etc.
    bool enabled = !ecs_has_id(world, e, EcsDisabled);
    if (igCheckbox("Enabled", &enabled)) {
        ecs_enable(world, e, enabled);
    }
    igSameLine(0, -1);
    if (igSmallButton("Delete")) {
        ecs_delete(world, e);
        return;
    }

    igBeginDisabled(!enabled);

    // Editable name; must not set name if another entity in the same scope already has the same name
    const char *currentName = getEntityLabel(world, e);
    igText(currentName);
    igSameLine(0, -1);
    if (igSmallButton("edit")) {
        igOpenPopup_Str("edit_entity_name", 0);
    }
    if (igBeginPopup("edit_entity_name", 0)) {
        static char newName[256];
        if (igIsWindowAppearing()) {
            strcpy(newName, currentName);
        }
        if (entityRenamer(world, ecs_get_target(world, e, EcsChildOf, 0), newName, 256)) {
            ecs_set_name(world, e, newName);
        }
        igEndPopup();
    }

    igSameLine(0, -1);
    igTextColored((ImVec4){0.5, 0.5, 0.5, 1}, "id: %u", e);
    // Display path if not in hierarchy root
    if (ecs_has_id(world, e, ecs_pair(EcsChildOf, EcsAny))) {
        // Remember to free string returned from ecs_get_fullpath
        char *path = ecs_get_fullpath(world, e);
        igSameLine(0, -1);
        igTextColored((ImVec4){0.5, 0.5, 0.5, 1}, path);
        ecs_os_free(path);
    }
    // TODO: detail info for entity flags?

    // TODO: button to quick query for children of this entity

    // Current Components, Tags, Relationships
    const ecs_type_t *type = ecs_get_type(world, e);
    for (int i = 0; i < type->count; i++) {
        ecs_id_t id = type->array[i];
        igPushID_Int(id);
        // Pairs don't pass ecs_is_valid, so check for pair first
        if (ecs_id_is_pair(id)) {
            // TODO: dedicated relationship inspector
            ecs_id_t first = ecs_pair_first(world, id);
            ecs_id_t second = ecs_pair_second(world, id);
            igText("Pair: ");
            igSameLine(0, -1);
            entityLabel(world, first);
            igSameLine(0, -1);
            if (igSmallButton("?##focus_first")) {
                ic->focusEntity = first;
            }
            igSameLine(0, -1);
            entityLabel(world, second);
            igSameLine(0, -1);
            if (igSmallButton("?##focus_second")) {
                ic->focusEntity = second;
            }
            igSameLine(0, -1);
            if (igSmallButton("x")) {
                ecs_remove_id(world, e, id);
                // need to skip component inspector from here, otherwise pairs with data will get immediately re-added
            } else if (ecs_has(world, first, EcsMetaType)) {
                void *componentData = ecs_get_mut_id(world, e, id);
                ecs_meta_cursor_t cur = ecs_meta_cursor(world, first, componentData);
                ecsMetaInspector(world, &cur, &ic->focusEntity);
            }
        } else if (ecs_is_valid(world, id)) {
            if (ecs_has(world, id, EcsComponent)) {
                componentInspector(world, e, id, &ic->focusEntity);
            } else {
                // TODO: dedicated tag inspector
                entityLabel(world, id);
                igSameLine(0, -1);
                if (igSmallButton("?")) {
                    ic->focusEntity = id;
                }
                igSameLine(0, -1);
                if (igSmallButton("x")) {
                    ecs_remove_id(world, e, id);
                }
            }
        } else {
            // Type may contain an invalid ID which is still meaningful (e.g. component override)
            igText("Unknown type: %u %x", id, id >> 32);
        }
        igPopID();
    }
    igEndDisabled();

    igSpacing();

    // Add Component
    static ecs_entity_t newComponent = 0;
    if (igButton("Add Component", (ImVec2){0, 0})) {
        if (ic->components == NULL) {
            // create component query
            ic->components = ecs_query_new(world, "EcsComponent");
        }
        igOpenPopup_Str("add_component_popup", 0);
        newComponent = 0;
    }
    if (igBeginPopup("add_component_popup", 0)) {
        if (entitySelector(world, ic->components, &newComponent)) {
            if (newComponent != 0) {
                ecs_add_id(world, e, newComponent);
            }
        }
        igEndPopup();
    }

    igSameLine(0, -1);
    // Add Tag
    static ecs_entity_t newTag = 0;
    if (igButton("Add Tag", (ImVec2){0, 0})) {
        if (ic->tags == NULL) {
            // create tag query (any entity, could be slow to evaluate)
            ic->tags = ecs_query_new(world, "_");
        }
        igOpenPopup_Str("add_tag_popup", 0);
        newTag = 0;
    }
    if (igBeginPopup("add_tag_popup", 0)) {
        if (entitySelector(world, ic->tags, &newTag)) {
            if (newTag != 0) {
                ecs_add_id(world, e, newTag);
            }
        }
        igEndPopup();
    }

    igSameLine(0, -1);
    // Add pair
    static ecs_id_t newPair = 0;
    if (igButton("Add Pair", (ImVec2){0, 0})) {
        if (ic->relationships == NULL) {
            // create relationshp query as union of all relationship properties. May still miss some entities that can be used as relationships, can handle these cases later with a more general purpose drag-and-drop relationship builder or something
            ic->relationships = ecs_query_new(world, "EcsComponent || EcsTag || EcsFinal || EcsDontInherit || EcsAlwaysOverride || EcsTransitive || EcsReflexive || EcsAcyclic || EcsTraversable || EcsExclusive || EcsUnion || EcsSymmetric || EcsWith || EcsOneOf");
        }
        igOpenPopup_Str("add_pair_popup", 0);
        newPair = 0;
    }
    if (igBeginPopup("add_pair_popup", 0)) {
        if (pairSelector(world, ic->relationships, &newPair)) {
            if (newPair != 0) {
                ecs_add_id(world, e, newPair);
            }
        }
        igEndPopup();
    }
}

void entityCreator(ecs_world_t *world, ecs_entity_t parent, EcsInspectionContext *ic) {
    static char name[256];
    // Within a popup, clear name when opening
    if (igIsWindowAppearing()) {
        name[0] = '\0';
    }
    if (entityRenamer(world, parent, name, 256)) {
        ecs_new_from_path(world, parent, name);
    }
}

bool entityRenamer(ecs_world_t *world, ecs_entity_t parent, char *name, size_t name_buf_size) {
    static bool doesNameConflict;

    // inputText should get default focus
    if (igIsWindowAppearing()) {
        doesNameConflict = false;
        igSetKeyboardFocusHere(0);
    }
    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
    bool nameSubmitted = igInputText("Name", name, name_buf_size, flags, NULL, NULL);

    // Check for collisions in parent scope
    if (igButton("Confirm", (ImVec2){0, 0}) || nameSubmitted) {
        ecs_entity_t existing = ecs_lookup_child(world, parent, name);
        if (existing == 0) {
            igCloseCurrentPopup();
            doesNameConflict = false;
            return true;
        } else {
            // Inform user that there is already a sibling with the same name
            doesNameConflict = true;
        }
    }
    if (doesNameConflict) {
        igTextColored((ImVec4){1.0, 0.0, 0.0, 1.0}, "Name must be unique within scope");
    }
    return false;
}

u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t *focus, bool defaultOpen) {
    u32 count = 1;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    // Highlight focused node
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (node == *focus) flags |= ImGuiTreeNodeFlags_Selected;

    bool drawChildren = false;
    ecs_iter_t children;
    // Attempting to search children for some Flecs builtins (., $) will cause an error, and others have no reason to display normally (_, *). Ignored list here is the same as the set checked in flecs_get_builtin
    if (node == EcsThis || node == EcsAny || node == EcsWildcard || node == EcsVariable) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    } else {
        children = ecs_children(world, node);
        // Display as a leaf if no children
        if (ecs_iter_is_true(&children)) {
            drawChildren = true;
        } else {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
    }

    // Note: leaf nodes are always *expanded*, not collapsed. Should skip iterating children, but still TreePop if leaf.
    bool expandNode = igTreeNodeEx_Str(getEntityLabel(world, node), flags);
    // If tree node was clicked, but not expanded, inspect node
    if (igIsItemClicked(ImGuiMouseButton_Left) && !igIsItemToggledOpen()) {
        *focus = node;
    }

    if (expandNode && drawChildren) {
        // Re-aquire iterator, because ecs_iter_is_true invalidates it
        children = ecs_children(world, node);
        while (ecs_children_next(&children)) {
            for (int i = 0; i < children.count; i++) {
                count += hierarchyInspector(world, children.entities[i], focus, defaultOpen);
            }
        }
    }
    if (expandNode) {
        igTreePop();
    }
    return count;
}

bool entitySelector(ecs_world_t *world, ecs_query_t *query, ecs_entity_t *selected) {
    bool selectionMade = false;

    // Set name filter
    if (igIsWindowAppearing()) {
        igSetKeyboardFocusHere(0);
    }
    static ImGuiTextFilter filter;
    ImGuiTextFilter_Draw(&filter, "##", 0);

    // List filtered results
    float itemHeight = igGetTextLineHeightWithSpacing();
    ecs_iter_t it = ecs_query_iter(world, query);
    entityList(world, &it, &filter, (ImVec2){200.0, itemHeight * 10.0}, 25, selected);

    // TODO: make fullpath inspector method
    char *path = ecs_get_fullpath(world, *selected);
    igTextColored((ImVec4){0.5, 0.5, 0.5, 1}, path);
    ecs_os_free(path);

    igBeginDisabled(*selected == 0);
    if (igButton("Select", (ImVec2){0, 0})) {
        igCloseCurrentPopup();
        selectionMade = true;
    }
    igSameLine(0, -1);
    igEndDisabled();
    if (igButton("Cancel", (ImVec2){0, 0})) {
        igCloseCurrentPopup();
        selectionMade = false;
    }

    return selectionMade;
}

bool pairSelector(ecs_world_t *world, ecs_query_t *relationshipQuery, ecs_id_t *selected) {
    // Show 2 entity selectors, preview relationship, return true when both are selected
    bool selectionMade = false;
    static ecs_entity_t relationship = 0;
    static ecs_entity_t target = 0;
    float itemHeight = igGetTextLineHeightWithSpacing();

    igPushID_Str("rel");
    igBeginGroup();
    // Set name filter
    if (igIsWindowAppearing()) {
        igSetKeyboardFocusHere(0);
    }
    static ImGuiTextFilter relationshipFilter;
    ImGuiTextFilter_Draw(&relationshipFilter, "##", 0);
    ecs_iter_t relationshipIter = ecs_query_iter(world, relationshipQuery);
    entityList(world, &relationshipIter, &relationshipFilter, (ImVec2){200.0, itemHeight * 10.0}, 25, &relationship);
    igEndGroup();
    igPopID();

    // Create term iterator for target based on OneOf property
    static ecs_iter_t targetIter;
    if (ecs_is_valid(world, relationship)) {
        if (ecs_has_id(world, relationship, EcsOneOf)) {
            targetIter = ecs_children(world, relationship);
        } else if (ecs_has_pair(world, relationship, EcsOneOf, EcsAny)) {
            targetIter = ecs_children(world, ecs_get_target(world, relationship, EcsOneOf, 0));
        } else {
            targetIter = ecs_term_iter(world, &(ecs_term_t){
                .id = ecs_childof(0)
            });
        }
    } else {
        // TODO: maybe just make null, and if null use tree inspector instead. Otherwise target selector is only useful with OneOf tagged relationships
        targetIter = ecs_term_iter(world, &(ecs_term_t){
            .id = ecs_childof(0)
        });
    }

    igSameLine(0, -1);

    igPushID_Str("target");
    igBeginGroup();
    static ImGuiTextFilter targetFilter;
    ImGuiTextFilter_Draw(&targetFilter, "##", 0);
    entityList(world, &targetIter, &targetFilter, (ImVec2){200.0, itemHeight * 10.0}, 25, &target);
    igEndGroup();
    igPopID();

    if (relationship != 0) {
        igText(getEntityLabel(world, relationship));
        igSameLine(0, -1);
    }
    igText(":");
    if (target != 0) {
        igSameLine(0, -1);
        igText(getEntityLabel(world, target));
    }

    igBeginDisabled(relationship == 0 || target == 0);
    if (igButton("Select", (ImVec2){0, 0})) {
        selectionMade = true;
        *selected = ecs_make_pair(relationship, target);
        igCloseCurrentPopup();
    }
    igSameLine(0, -1);
    igEndDisabled();
    if (igButton("Cancel", (ImVec2){0, 0})) {
        selectionMade = false;
        igCloseCurrentPopup();
    }

    return selectionMade;
}

bool entityList(ecs_world_t *world, ecs_iter_t *iter, ImGuiTextFilter *filter, ImVec2 size, s32 maxDisplayedResults, ecs_entity_t *selected) {
    bool anyClicked = false;
    s32 fullCount = 0;
    s32 displayedCount = 0;

    igBeginChild_Str("Entity Selector", size, true, ImGuiWindowFlags_HorizontalScrollbar);
    while (ecs_iter_next(iter)) {
        for (int i = 0; i < iter->count; i++) {
            if (displayedCount < maxDisplayedResults) {
                ecs_entity_t e = iter->entities[i];
                const char *name = getEntityLabel(world, e);
                // TODO: make filter optional?
                if (ImGuiTextFilter_PassFilter(filter, name, NULL)) {
                    igPushID_Int(e); // Ensure that identical names in different scopes don't conflict
                    // NOTE: igSelectable will, by default, call CloseCurrentPopup when clicked. Set flag to disable this behavior
                    if (igSelectable_Bool(name, e == *selected, ImGuiSelectableFlags_DontClosePopups, (ImVec2){0, 0})) {
                        *selected = e;
                        anyClicked = true;
                    }
                    igPopID();
                    displayedCount++;
                }
            }
            fullCount++;
        }
    }
    // FIXME: extremely minor, but this will display '0 more results' when fullCount is exactly maxDisplayedResults
    if (displayedCount == maxDisplayedResults) {
        igTextColored((ImVec4){0.5, 0.5, 0.5, 1.0}, "%i more results...", fullCount - maxDisplayedResults);
    }
    igEndChild();
    return anyClicked;
}

void componentInspector(ecs_world_t *world, ecs_entity_t e, ecs_entity_t component, ecs_entity_t *focusEntity) {

    igBeginGroup();
    entityLabel(world, component);
    if (igIsItemHovered(0)) {
        igSetTooltip(ecs_get_fullpath(world, component));
    }
    igSameLine(0, -1);
    if (igSmallButton("?")) {
        *focusEntity = component;
    }
    igSameLine(0, -1);
    if (igSmallButton("x")) {
        ecs_remove_id(world, e, component);
        // Early return; because this remove happens immediately, need to avoid unintended changes to this component
        igEndGroup();
        return;
    }

    // TODO: custom inspectors for specific component types, like QueryDesc. Is this the right place to handle this behavior?
    if (component == ecs_id(QueryDesc)) {
        igSameLine(0, -1);

        //QueryDesc *qd = ecs_get_mut(world, e, QueryDesc);
        // button opens input for new query expression, will create description from expression
        if (igButton("Set Query Expression", (ImVec2){0, 0})) {
            igOpenPopup_Str("query_expr", 0);
        }
        if (igBeginPopup("query_expr", 0)) {
            static char queryExpr[MAX_QUERY_EXPR_LENGTH] = {0};
            //static char queryExprError[MAX_QUERY_EXPR_LENGTH]; // Unneeded? not creating the query here, so might not get direct error feedback
            // if (igIsWindowAppearing()) {
            //     queryExpr[0] = '\0';
            // }
            // TODO: make input field larger
            if (igInputText("Query", queryExpr, MAX_QUERY_EXPR_LENGTH, ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL)) {
                //qd->desc.filter.expr = queryExpr;
                ecs_set(world, e, QueryDesc, {.desc.filter.expr = queryExpr});
                igCloseCurrentPopup();
            }
            igEndPopup();
        }
    } else if (component == ecs_id(Color)) {
        igSameLine(0, -1);
        // Color picker
        static ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_PickerHueWheel;
        Color *col = ecs_get_mut(world, e, Color);
        igColorEdit4("##color_picker", col->v, flags);
    } else if (ecs_has(world, component, EcsMetaType)) {
        // NOTE: calling ecs_get_mut_id AFTER a call to ecs_remove_id will end up adding the removed component again, with default values
        void *componentData = ecs_get_mut_id(world, e, component);
        ecs_meta_cursor_t cur = ecs_meta_cursor(world, component, componentData);
        ecsMetaInspector(world, &cur, focusEntity);
    } else {
        // TODO: make it clear that this is a component with no meta info available, still show some basic params like size if possible
    }

    igEndGroup();
}

void ecsMetaInspector(ecs_world_t *world, ecs_meta_cursor_t *cursor, ecs_entity_t *focusEntity) {
    ecs_entity_t type = ecs_meta_get_type(cursor);

    igPushID_Int(type);
    igIndent(0);
    ecs_meta_push(cursor);

    int i = 0;
    do {
        igPushID_Int(i);
        type = ecs_meta_get_type(cursor);
        if (type != 0) {
            igText("%s (%s): ", ecs_meta_get_member(cursor), ecs_get_name(world, type));
            ecs_type_kind_t kind = ecs_get(world, type, EcsMetaType)->kind;
            switch (kind) {
                case EcsPrimitiveType:
                    igSameLine(0, -1);
                    ecs_primitive_kind_t primKind = ecs_get(world, type, EcsPrimitive)->kind;
                    primitiveInspector(world, primKind, ecs_meta_get_ptr(cursor), focusEntity);
                    break;
                case EcsStructType:
                    // recurse componentInspector
                    ecsMetaInspector(world, cursor, focusEntity);
                    break;
                default:
                    // TODO priority order: bitmask, enum, array, vector, opaque
                    igSameLine(0, -1);
                    igText("Unhandled type kind");
                    break;
            }
        }
        igPopID();
        i++;
    } while (ecs_meta_try_next(cursor));

    ecs_meta_pop(cursor);
    igUnindent(0);
    igPopID();
}

void primitiveInspector(ecs_world_t *world, ecs_primitive_kind_t kind, void *field, ecs_entity_t *focusEntity) {
    switch (kind) {
        case EcsBool:
            igCheckbox("##", field);
        case EcsChar:
            igInputScalar("##", ImGuiDataType_U8, field, NULL, NULL, "%c", 0);
            break;
        case EcsByte:
            igInputScalar("##", ImGuiDataType_U8, field, NULL, NULL, "%x", 0);
            break;
        case EcsU8:
            igInputScalar("##", ImGuiDataType_U8, field, NULL, NULL, NULL, 0);
            break;
        case EcsU16:
            igInputScalar("##", ImGuiDataType_U16, field, NULL, NULL, NULL, 0);
            break;
        case EcsU32:
            igInputScalar("##", ImGuiDataType_U32, field, NULL, NULL, NULL, 0);
            break;
        case EcsU64:
            igInputScalar("##", ImGuiDataType_U64, field, NULL, NULL, NULL, 0);
            break;
        case EcsI8:
            igInputScalar("##", ImGuiDataType_S8, field, NULL, NULL, NULL, 0);
            break;
        case EcsI16:
            igInputScalar("##", ImGuiDataType_S16, field, NULL, NULL, NULL, 0);
            break;
        case EcsI32:
            igInputInt("##", field, 1, 10, 0);
            break;
        case EcsI64:
            igInputScalar("##", ImGuiDataType_S32, field, NULL, NULL, NULL, 0);
            break;
        case EcsF32:
            igDragFloat("##", field, 0.02f, -FLT_MAX, FLT_MAX, "%.3f", 0);
            break;
        case EcsF64:
            igDragScalar("##", ImGuiDataType_Double, field, 0.02f, NULL, NULL, "%.6g", 0);
            break;
        case EcsUPtr:
            igInputScalar("##", ImGuiDataType_U64, field, NULL, NULL, "%x", 0);
            break;
        case EcsIPtr:
            igInputScalar("##", ImGuiDataType_S64, field, NULL, NULL, "%x", 0);
            break;
        case EcsString:
            igText(field);
            //igInputText("##", field, 0, 0, NULL, NULL); // TODO set size
            break;
        case EcsEntity:
            ecs_entity_t e = *(ecs_entity_t*)(field);
            entityLabel(world, e);
            if (focusEntity != NULL) {
                igSameLine(0, -1);
                if (igSmallButton("?")) {
                    *focusEntity = e;
                }
            }
            break;
        default:
            igText("Error: unhandled primitive type");
            break;
    }
}

const char *getEntityLabel(ecs_world_t *world, ecs_entity_t e) {
    if (ecs_is_valid(world, e)) {
        const char *name = ecs_get_name(world, e);
        if (name == NULL) {
            static char idStr[64];
            sprintf(idStr, "%lu", e);
            name = idStr;
        }
        return name;
    } else {
        return "INVALID ENTITY";
    }
}

void entityLabel(ecs_world_t *world, ecs_entity_t e) {
    if (ecs_is_valid(world, e)) {
        const char *name = ecs_get_name(world, e);
        if (name == NULL) {
            igText("%lu (unnamed)", e);
        } else {
            igText(name);
        }
    } else {
        igText("INVALID ENTITY %lu", e);
    }

    // Consider enabling this for extra info
    // if (igIsItemHovered(0)) {
    //     char *tooltip = ecs_get_fullpath(world, e);
    //     igSetTooltip(tooltip);
    //     ecs_os_free(tooltip);
    // }
}



void possessEntity(ecs_world_t *world, ecs_entity_t target) {
    // TODO: rethink this to work with Ego system
    // Way of doing this that doesn't rely on storing a reference to the 'active character' in uistate: use filters and systems
    // When switching control, first remove PlayerControlled tag from all entities. Then add the tag to target entity
    if (ecs_is_valid(world, target)) {
        ecs_iter_t it = ecs_term_iter(world, &(ecs_term_t){.id = ecs_pair(Ego, EcsAny)});
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
        ecs_set_pair(world, target, Ego, 0); // TODO: can't have a relationship where the target is 0; should create some kind of 'blank' ego for non-ai controlled characters
        ecs_remove_pair(world, target, Ego, 0);
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
