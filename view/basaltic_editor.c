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

typedef struct {
    ecs_entity_t focusEntity;
    // TODO: add focus history, change focus by writing to / cycling through history. Add back/forward buttons or recently viewed list
    ecs_query_t *components;
    ecs_query_t *relationships;
    ecs_query_t *query; // user defined query
    s32 queryPageLength;
    s32 queryPage;
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
void ecsEntityInspector(ecs_world_t *world, EcsInspectionContext *ic);
// Returns number of entities in hierarchy, including the root
u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t *focus, bool defaultOpen);
bool entitySelector(ecs_world_t *world, ecs_query_t *query, ecs_entity_t *selected);
bool pairSelector(ecs_world_t *world, ecs_query_t *relationshipQuery, ecs_query_t *targetQuery, ecs_id_t *selected);
void componentInspector(ecs_world_t *world, ecs_entity_t e, ecs_entity_t component, ecs_entity_t *focusEntity);
void ecsMetaInspector(ecs_world_t *world, ecs_meta_cursor_t *cursor, ecs_entity_t *focusEntity);
// If kind is EcsEntity and focus entity is not NULL, will add a button to set focusEntity to component value
void primitiveInspector(ecs_world_t *world, ecs_primitive_kind_t kind, void *field, ecs_entity_t *focusEntity);
// Return entity name if it is named, otherwise return stringified ID (ID string is shared between calls, use immediately or copy to preserve)
const char *getEntityLabel(ecs_world_t *world, ecs_entity_t e);
// Display entity name if it is named, otherwise display id
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
        .queryPageLength = 20,
        .queryExpr = "Camera"
    };
    modelInspector = (EcsInspectionContext){
        .worldName = "Model",
        .queryPageLength = 20,
        .queryExpr = "Position"
    };
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

    ImVec2 avail;
    igGetContentRegionAvail(&avail);
    if(igBeginChild_Str("Query", (ImVec2){avail.x * 0.5, 0}, true, ImGuiWindowFlags_MenuBar)) {
        if (igBeginMenuBar()) {
            igText("Custom Query");
            igEndMenuBar();
        }
        ecsQueryInspector(world, ic);
    }
    igEndChild();

    igSameLine(0, -1);

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
        ic->queryPage = 0;
    }
    if (ic->queryExprError[0] != '\0') {
        igTextColored((ImVec4){1.0, 0.0, 0.0, 1.0}, ic->queryExprError);
    }
    igInputInt("Results per page", &ic->queryPageLength, 1, 1, 0);
    ic->queryPageLength = max_int(ic->queryPageLength, 1);

    // TODO: Put in a scroll area if list too long
    if (ic->query != NULL) {
        igPushID_Str("Query Results");
        u32 resultsCount = 0;
        u32 minDisplayed = ic->queryPageLength * ic->queryPage;
        u32 maxDisplayed = minDisplayed + ic->queryPageLength;
        ecs_iter_t it = ecs_query_iter(world, ic->query);
        while (ecs_query_next(&it)) {
            for (int i = 0; i < it.count; i++) {
                //resultsCount += hierarchyInspector(world, it.entities[i], ic);
                if (resultsCount >= minDisplayed && resultsCount < maxDisplayed) {
                    igPushID_Int(it.entities[i]);
                    // TODO: instead of hierarchy inspector, use query fields to display table of filter term components
                    hierarchyInspector(world, it.entities[i], &ic->focusEntity, false);
                    igPopID();
                }
                resultsCount++;
            }
        }
        // TODO: after converting to table, add blank rows so other controls don't change position
        igValue_Uint("Results", resultsCount);
        s32 pageCount = (s32)ceilf((float)resultsCount / ic->queryPageLength) - 1;
        if (pageCount > 1) {
            igSliderInt("Page", &ic->queryPage, 0, pageCount, NULL, ImGuiSliderFlags_AlwaysClamp);
        }
        ic->queryPage = min_int(ic->queryPage, pageCount);
        igPopID();
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
        static bool doesNameConflict;
        if (igIsWindowAppearing()) {
            strcpy(newName, currentName);
            doesNameConflict = false;
            igSetKeyboardFocusHere(0);
        }
        const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
        if (igInputText("##rename_entity", newName, 256, flags, NULL, NULL)) {
            // Check if name matches existing entity in the same scope TODO
            ecs_entity_t parent = ecs_get_target(world, e, EcsChildOf, 0);
            ecs_entity_t existing = ecs_lookup_child(world, parent, newName);
            if (existing == 0) {
                ecs_set_name(world, e, newName);
                igCloseCurrentPopup();
            } else {
                // Inform user that there is already a sibling with the same name
                doesNameConflict = true;
            }
        }
        if (doesNameConflict) {
            igTextColored((ImVec4){1.0, 0.0, 0.0, 1.0}, "Name must be unique within scope");
        }
        igEndPopup();
    }

    // Display path if not in hierarchy root
    if (ecs_has_id(world, e, ecs_pair(EcsChildOf, EcsAny))) {
        igSameLine(0, -1);
        igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4){0.5, 0.5, 0.5, 1});
        // Remember to free string returned from ecs_get_fullpath
        char *path = ecs_get_fullpath(world, e);
        igText(path);
        ecs_os_free(path);
        igPopStyleColor(1);
    }

    // Current Components, Tags, Relationships
    const ecs_type_t *type = ecs_get_type(world, e);
    for (int i = 0; i < type->count; i++) {
        ecs_id_t id = type->array[i];
        igPushID_Int(id);
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
            }
            if (ecs_has(world, first, EcsMetaType)) {
                void *componentData = ecs_get_mut_id(world, e, id);
                ecs_meta_cursor_t cur = ecs_meta_cursor(world, first, componentData);
                ecsMetaInspector(world, &cur, &ic->focusEntity);
            }

        } else if (ecs_has(world, id, EcsComponent)) {
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
        igPopID();
    }
    igEndDisabled();

    igSpacing();

    // Add new Component, Tag, or Relationship
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
    static ecs_id_t newPair = 0;
    if (igButton("Add Pair", (ImVec2){0, 0})) {
        if (ic->relationships == NULL) {
            // create component query
            ic->relationships = ecs_query_new(world, "EcsComponent || EcsTag");
        }
        igOpenPopup_Str("add_pair_popup", 0);
        newPair = 0;
    }
    if (igBeginPopup("add_pair_popup", 0)) {
        if (pairSelector(world, ic->relationships, ic->query, &newPair)) {
            if (newPair != 0) {
                ecs_add_id(world, e, newPair);
            }
        }
        igEndPopup();
    }
}

u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t *focus, bool defaultOpen) {
    u32 count = 1;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    // Highlight focused node
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (node == *focus) flags |= ImGuiTreeNodeFlags_Selected;

    ecs_iter_t children = ecs_children(world, node);
    // Display as a leaf if no children
    if (!ecs_iter_is_true(&children)) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }


    bool expandNode = igTreeNodeEx_Str(getEntityLabel(world, node), flags);
    // If tree node was clicked, but not expanded, inspect node
    if (igIsItemClicked(ImGuiMouseButton_Left) && !igIsItemToggledOpen()) {
        *focus = node;
    }

    if (expandNode) {
        // Re-aquire iterator, because ecs_iter_is_true invalidates it
        children = ecs_children(world, node);
        while (ecs_children_next(&children)) {
            for (int i = 0; i < children.count; i++) {
                count += hierarchyInspector(world, children.entities[i], focus, defaultOpen);
            }
        }
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
    ImGuiTextFilter_Draw(&filter, "Filter", 0);

    // List filtered results
    // TODO: better way to set size?
    igBeginChild_Str("Entity Selector", (ImVec2){200, 250}, true, ImGuiWindowFlags_HorizontalScrollbar);
    ecs_iter_t it = ecs_query_iter(world, query);
    while (ecs_query_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            ecs_entity_t e = it.entities[i];
            const char *name = getEntityLabel(world, e);
            if (ImGuiTextFilter_PassFilter(&filter, name, NULL)) {
                // NOTE: igSelectable will, by default, call CloseCurrentPopup when clicked. Use flags to disable this behavior
                if (igSelectable_Bool(name, e == *selected, ImGuiSelectableFlags_DontClosePopups, (ImVec2){0, 0})) {
                    *selected = e;
                }
            }
        }
    }
    igEndChild();

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

bool pairSelector(ecs_world_t *world, ecs_query_t *relationshipQuery, ecs_query_t *targetQuery, ecs_id_t *selected) {
    // Show 2 entity selectors, preview relationship, return true when both are selected
    bool selectionMade = false;
    static ecs_entity_t relationship = 0;
    static ecs_entity_t target = 0;

    igPushID_Str("rel");
    igBeginGroup();
    entitySelector(world, relationshipQuery, &relationship);
    igEndGroup();
    igPopID();

    igSameLine(0, -1);

    igPushID_Str("target");
    igBeginGroup();
    entitySelector(world, targetQuery, &target);
    igEndGroup();
    igPopID();

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
    }

    // early return if no meta info is available
    if (!ecs_has(world, component, EcsMetaType)) {
        igEndGroup();
        return;
    } else {
        void *componentData = ecs_get_mut_id(world, e, component);
        ecs_meta_cursor_t cur = ecs_meta_cursor(world, component, componentData);
        ecsMetaInspector(world, &cur, focusEntity);
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
