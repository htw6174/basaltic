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
#include "basaltic_worldGen.h"
#include "basaltic_phases_view.h"
#include "basaltic_components_view.h"
#include "basaltic_components.h"
#include "basaltic_sokol_gfx.h"
#include "flecs.h"
#include "bc_flecs_utils.h"
#include "khash.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

// #define SOKOL_GLCORE33
// #include "sokol_gfx.h"
// #define SOKOL_IMGUI_NO_SOKOL_APP
// #define SOKOL_IMGUI_IMPL
// #include "util/sokol_imgui.h"

#define MAX_CUSTOM_QUERIES 4
#define STRING_BUFFER_SIZE 4096

// Colors
#define IG_COLOR_DEFAULT ((ImVec4){1.0, 1.0, 1.0, 1.0})
#define IG_COLOR_DISABLED ((ImVec4){1.0, 1.0, 1.0, 0.5})
#define IG_COLOR_WARNING ((ImVec4){0.9, 0.7, 0.0, 1.0})
#define IG_COLOR_ERROR ((ImVec4){1.0, 0.0, 0.0, 1.0})
// Flecs Explorer color defaults:
// Module: Yellow
// Component: Blue (Tags are also blue for some reason)
// Prefab: White
// Other: Green
// Also adding Relationship: Teal
#define IG_COLOR_MODULE ((ImVec4){0.9, 0.9, 0.0, 1.0})
#define IG_COLOR_COMPONENT ((ImVec4){0.5, 0.5, 1.0, 1.0})
#define IG_COLOR_RELATIONSHIP ((ImVec4){0.6, 0.9, 0.9, 1.0})
#define IG_COLOR_PREFAB ((ImVec4){1.0, 1.0, 1.0, 1.0})
#define IG_COLOR_ENTITY ((ImVec4){0.0, 0.85, 0.0, 1.0})

#define IG_SIZE_DEFAULT ((ImVec2){0.0, 0.0})


typedef struct {
    ecs_query_t *query;
    s32 queryPage;
    s32 queryPageLength;
    char queryExpr[MAX_QUERY_EXPR_LENGTH];
    char queryExprError[MAX_QUERY_EXPR_LENGTH];
    ImGuiTextFilter filter;
} QueryContext;

typedef struct {
    ecs_entity_t focusEntity;
    // TODO: add focus history, change focus by writing to / cycling through history. Add back/forward buttons or recently viewed list
    // Cached queries for adding types to entities
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
    char newGameSeed[256];
} EditorContext;

static EditorContext ec;
static EcsInspectionContext viewInspector;
static EcsInspectionContext modelInspector;

// workspace for editable strings
static char stringEditBuffer[STRING_BUFFER_SIZE];


/* ECS General-purpose */
void ecsWorldInspector(ecs_world_t *world, EcsInspectionContext *ic);
void ecsQueryColumns(ecs_world_t *world, EcsInspectionContext *ic);
void ecsQueryInspector(ecs_world_t *world, QueryContext *qc, ecs_entity_t *selected);
void ecsTreeInspector(ecs_world_t *world, ecs_entity_t *selected);
void ecsEntityInspector(ecs_world_t *world, EcsInspectionContext *ic);

void entityCreator(ecs_world_t *world, ecs_entity_t parent, ecs_entity_t *focusEntity);
/** Displays an igInputText for [name]]. Returns true when [name] is set to a unique name in parent scope and submitted. When used within a popup, auto-focuses input field and closes popup when confirming. */
bool entityRenamer(ecs_world_t *world, ecs_entity_t parent, char *name, size_t name_buf_size);
/**
 * @brief Displays a tree of entity labels starting from node and traversing given relationship downward.
 *
 * @param world
 * @param node root entity for tree
 * @param relationship relationship to traverse. Should be Acyclic
 * @param focus set to clicked entity, if any
 * @param defaultOpen if true, automatically expands all nodes. Must be false if relationship is not Acyclic
 * @return number of entities in tree, including root node
 */
u32 relationshipTreeInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t relationship, ecs_entity_t *focus, bool defaultOpen);
/// Displays a tree of entity labels starting from node and traversing children downward. Returns number of entities in hierarchy, including the root node
u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t *focus, bool defaultOpen);
bool entitySelector(ecs_world_t *world, ecs_query_t *query, ecs_entity_t *selected);
bool pairSelector(ecs_world_t *world, ecs_query_t *relationshipQuery, ecs_id_t *selected);
/** Displays query results as a scrollable list of igSelectable. If filter is not NULL, filters list by entity name. Sets [selected] to id of selected item. Returns true when an item is clicked */
bool entityList(ecs_world_t *world, ecs_iter_t *it, ImGuiTextFilter *filter, ImVec2 size, s32 pageLength, s32 *pageNumber, ecs_entity_t *selected);

void ecsPairWidget(ecs_world_t *world, ecs_entity_t e, ecs_id_t pair, ecs_entity_t *focusEntity);
void componentInspector(ecs_world_t *world, ecs_entity_t e, ecs_entity_t component, ecs_entity_t *focusEntity);
bool ecsMetaInspector(ecs_world_t *world, ecs_meta_cursor_t *cursor, ecs_entity_t *focusEntity);
/** If kind is EcsEntity and focus entity is not NULL, will add a button to set focusEntity to component value */
bool primitiveInspector(ecs_world_t *world, ecs_primitive_kind_t kind, void *field, ecs_entity_t *focusEntity);
/** Return entity name if it is named, otherwise return stringified ID (ID string is shared between calls, use immediately or copy to preserve) */
const char *getEntityLabel(ecs_world_t *world, ecs_entity_t e);
/** Display entity name if it is named, otherwise display id */
void entityLabel(ecs_world_t *world, ecs_entity_t e);
bool entityButton(ecs_world_t *world, ecs_entity_t e);

/* Specalized Inspectors */
void modelSetupInspector(bc_SupervisorInterface *si);
void modelWorldInspector(ecs_world_t *modelWorld, ecs_world_t *viewWorld);
/** Returns true if the cell was altered */
bool cellInspector(ecs_world_t *world, ecs_entity_t plane, htw_geo_GridCoord coord, ecs_entity_t *focusEntity);
void bitmaskToggle(const char *prefix, u32 *bitmask, u32 toggleBit);
void dateTimeInspector(u64 step);
void coordInspector(const char *label, htw_geo_GridCoord coord);
void renderTargetInspector(ecs_world_t *world);
void toolPaletteInspector(ecs_world_t *world);

/* Custom component inspectors */

/**
 * @brief Callback for specialized handling of a component when viewed in the editor. Should return true if component was modified.
 *
 */
typedef bool (*bc_componentInspector)(ecs_world_t *world, ecs_entity_t e, ecs_entity_t component);

KHASH_MAP_INIT_INT(CustomInspector, bc_componentInspector);
/// hashmap from Component entity ids to the inspector callback they should use
static khash_t(CustomInspector) *customInspectors;

bool queryDescriptionInspector(ecs_world_t *world, ecs_entity_t e, ecs_entity_t component);
bool colorInspector(ecs_world_t *world, ecs_entity_t e, ecs_entity_t component);

/* Misc Functions*/
/** Create query valid for iterating over results in an entity list, which adds several optional terms for prettifying results and includes disabled entities */
ecs_query_t *createInspectorQuery(ecs_world_t *world, const char *expr);
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
            [0] = {.queryExpr = "bcview.Tool"},
            [1] = {.queryExpr = "Pipeline || InstanceBuffer"}, //(bcview.RenderPipeline, _)"},
            [2] = {.queryExpr = "Prefab, !Prefab(up(ChildOf))"}, // only display top-level prefabs
            [3] = {.queryExpr = "flecs.system.System"},
        }
    };

    modelInspector = (EcsInspectionContext){0};

    //simgui_setup(&(simgui_desc_t){0});

    // setup custom inspector callback map
    customInspectors = kh_init(CustomInspector);
    // NOTE: should consider doing this setup elsewhere. Currently it relies on at least one ECS world being initialized before editor setup, which isn't guaranteed.
    int i, absent;
    i = kh_put(CustomInspector, customInspectors, ecs_id(QueryDesc), &absent);
    kh_val(customInspectors, i) = queryDescriptionInspector;
    i = kh_put(CustomInspector, customInspectors, ecs_id(Color), &absent);
    kh_val(customInspectors, i) = colorInspector;
}

void bc_teardownEditor(void) {
    //simgui_shutdown();
    kh_destroy(CustomInspector, customInspectors);
}

void bc_drawEditor(bc_SupervisorInterface *si, bc_ModelContext *model, ecs_world_t *viewWorld, bc_UiState *ui)
{
    /*
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
    */

    if (igBegin("View Inspector", NULL, ImGuiWindowFlags_None)) {
        //renderTargetInspector(viewWorld);
        ecsWorldInspector(viewWorld, &viewInspector);
    }
    igEnd();


    igBegin("Model Inspector", NULL, ImGuiWindowFlags_None);
    if (model == NULL) {
        modelSetupInspector(si);
    } else {
        if (igButton("Stop model", (ImVec2){0, 0})) {
            si->signal = BC_SUPERVISOR_SIGNAL_STOP_MODEL;
        }

        /* World info that is safe to inspect at any time: */
        // TODO: move this into locked area now that this info is stored in ecs
        // igText("Seed string: %s", world->seedString);
        // igValue_Uint("World seed", world->seed);
        igValue_Uint("Logic step", model->step);
        dateTimeInspector(model->step);

        ModelStepControl *stepper = ecs_singleton_get_mut(viewWorld, ModelStepControl);
        igPushItemWidth(200);

        // Tick rate slider TODO: change for new runforsteps method of scheduling model steps
        //igSliderInt("Min Tick Duration", (int*)&model->tickInterval, 0, 1000, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
        igSliderInt("Step batch size", (int*)&stepper->stepsPerRun, 1, 10000, "%d", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
        igSliderInt("Batch every n frames", (int*)&stepper->framesBetweenRuns, 1, 60, "%d", ImGuiSliderFlags_AlwaysClamp);
        igPopItemWidth();

        if (igButton("Advance logic step by batch size", (ImVec2){0, 0})) {
            stepper->doSingleRun = true;
        }
        // TODO: can now add an easy "Run for n Steps" button
        igSameLine(0, -1);
        if (igButton("Start auto step", (ImVec2){0, 0})) {
            stepper->doAuto = true;
        }
        igSameLine(0, -1);
        if (igButton("Stop auto step", (ImVec2){0, 0})) {
            stepper->doAuto = false;
        }

        ecs_singleton_modified(viewWorld, ModelStepControl);

        igSpacing();

        /* Don't inspect model while it's running */
        if (SDL_TryLockMutex(model->mutex) == 0) {
            ecs_world_t *modelWorld = model->world;
            // set model ecs world scope, to keep view's external tags/queries separate
            ecs_entity_t oldScope = ecs_get_scope(modelWorld);
            ecs_entity_t viewScope = ecs_entity_init(modelWorld, &(ecs_entity_desc_t){.name = "bcview"});
            ecs_set_scope(modelWorld, viewScope);
            modelWorldInspector(modelWorld, viewWorld);
            ecsWorldInspector(modelWorld, &modelInspector);
            ecs_set_scope(modelWorld, oldScope);
            SDL_UnlockMutex(model->mutex);
        }
    }
    igEnd();
}

void bc_drawGUI(bc_SupervisorInterface *si, bc_ModelContext *model, ecs_world_t *viewWorld) {
    // enable docking over entire screen
    igDockSpaceOverViewport(igGetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, ImGuiWindowClass_ImGuiWindowClass());

    if (model == NULL) {
        if (igBegin("New World", NULL, 0)) {
            modelSetupInspector(si);
        }
        igEnd();
    } else {
        ecs_world_t *modelWorld = NULL;
        if (SDL_TryLockMutex(model->mutex) == 0) {
            modelWorld = model->world;
            // NOTE: not good practice to have the unlock outside lock scope; will improve when eventually reorganizing GUI
        }
        if (igBegin("Settings", NULL, 0)) {
            if (igButton("Create new world", (ImVec2){0, 0})) {
                si->signal = BC_SUPERVISOR_SIGNAL_STOP_MODEL;
            }
            // TODO: inspect seed and step
            //igText("Seed: %s", worldState->seedString);
            dateTimeInspector(model->step);
            igPushItemWidth(200);
            //int tps = 1000 / MAX(model->tickInterval, 1);
            //tps = model->tickInterval == 0 ? 10000 : tps;
            //char *ticksFormat = model->tickInterval == 0 ? "Unlimited" : "%d";
            //igSliderInt("Max Ticks per Second", &tps, 1, 10000, ticksFormat, ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
            //model->tickInterval = 1000 / tps;
            igPopItemWidth();

            // TODO: need a better solution for non-guaranteed entities like this in the final GUI. Currently, this relies on order of setup and some menus being default collapsed to avoid crash from failed lookup
            ecs_entity_t terrainPipeline = ecs_lookup_fullpath(viewWorld, "bcview.GBufferPass.Terrain Pipeline");

            if (igCollapsingHeader_TreeNodeFlags("Video Settings", ImGuiTreeNodeFlags_None)) {
                // Toggle shadows
                bool shadows = !ecs_has_id(viewWorld, OnPassShadow, EcsDisabled);
                if (igCheckbox("Enable Shadows", &shadows)) {
                    ecs_enable(viewWorld, OnPassShadow, shadows);
                }
                // tile borders
                TerrainPipelineUniformsFrag *tf = ecs_get_mut(viewWorld, terrainPipeline, TerrainPipelineUniformsFrag);
                if (igCheckbox("Show tile borders", &tf->drawBorders)) {
                    ecs_modified(viewWorld, terrainPipeline, TerrainPipelineUniformsFrag);
                }
                // Render scale
                RenderScale *rs = ecs_get_mut(viewWorld, VideoSettings, RenderScale);
                if (igSliderFloat("RenderScale", rs, 0.05, 1.0, "%.2f", 0)) {
                    ecs_modified(viewWorld, VideoSettings, RenderScale);
                }
                // Sun
                SunLight *sun = ecs_singleton_get_mut(viewWorld, SunLight);
                igSliderFloat("Sun Inclination", &sun->inclination, -90.0, 90.0, "%.1f", 0);
                igSliderFloat("Sun Azimuth", &sun->azimuth, -360.0, 360.0, "%.1f", 0);
                ecs_singleton_modified(viewWorld, SunLight);

            }

            // TODO
            //if (igCollapsingHeader_TreeNodeFlags("Scripting", ImGuiTreeNodeFlags_None)) {}


            if (igCollapsingHeader_TreeNodeFlags("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                igText("Spacebar: advance time 1 hour or pause time");
                igText("p: auto advance time at max ticks per second");
                igText("w/a/s/d: pan camera");
                igText("q/e/r/f: rotate camera");
                igText("scroll wheel or z/x: zoom in and out");
                igText("`: enable full editor (WIP)");
            }

            igSpacing();

            // Tool tabs
            igText("Tool mode: ");
            if (igBeginTabBar("Tools", ImGuiTabBarFlags_None)) {
                // Lookup entities to bind to tab state
                ecs_entity_t camBindGroup = ecs_lookup_fullpath(viewWorld, "bcview.groups.camera_mouse");
                ecs_entity_t terrainBindGroup = ecs_lookup_fullpath(viewWorld, "bcview.groups.terrain_mouse");
                ecs_entity_t riverBindGroup = ecs_lookup_fullpath(viewWorld, "bcview.groups.river_mouse");
                ecs_entity_t actorBindGroup = ecs_lookup_fullpath(viewWorld, "bcview.groups.actor_mouse");
                ecs_entity_t playerBindGroup = ecs_lookup_fullpath(viewWorld, "bcview.groups.player_mouse");
                if (camBindGroup) {
                    bool camBindActive = igBeginTabItem("Camera", NULL, ImGuiTabItemFlags_None);
                    if (camBindActive) {
                        igText("Left click and drag to pan");
                        igText("Right click and drag to orbit");
                        // Sensitivity settings
                        MousePreferences *mp = ecs_singleton_get_mut(viewWorld, MousePreferences);
                        igSliderFloat("Mouse sensitivity", &mp->sensitivity, 1.0, 1000.0, "%.1f", ImGuiSliderFlags_Logarithmic);
                        igCheckbox("Flip Horizontal", &mp->invertX);
                        igCheckbox("Flip Vertical", &mp->invertY);

                        ecs_singleton_modified(viewWorld, MousePreferences);
                        igEndTabItem();
                    }
                    ecs_enable(viewWorld, camBindGroup, camBindActive);
                }
                if (terrainBindGroup) {
                    bool terrainBindActive = igBeginTabItem("Edit Terrain", NULL, ImGuiTabItemFlags_None);
                    if (terrainBindActive) {
                        ecs_entity_t brushField = ecs_get_target(viewWorld, ecs_id(AdditiveBrush), BrushField, 0);
                        const char *fieldName = getEntityLabel(viewWorld, brushField);
                        igText("Left click and drag to increase %s", fieldName);
                        igText("Right click and drag to decrease %s", fieldName);
                        igText("Middle mouse and drag to set exact %s value", fieldName);
                        // Brush settings
                        AdditiveBrush *ab = ecs_singleton_get_mut(viewWorld, AdditiveBrush);
                        BrushSize *bs = ecs_get_mut(viewWorld, ecs_id(AdditiveBrush), BrushSize);
                        const EcsMemberRanges *ranges = ecs_get(viewWorld, brushField, EcsMemberRanges);
                        int min, max;
                        if (ranges != NULL) {
                            // NOTE: imgui sliders extents are limited to half the natural type range for whatever type they are
                            min = 1; //ranges->value.min;
                            max = MIN(ranges->value.max, INT32_MAX / 2);
                        } else {
                            min = 1;
                            max = 100;
                        }
                        igSliderInt("Brush Strength", &ab->value, min, max, "%d",
                                    ImGuiSliderFlags_AlwaysClamp); // | ImGuiSliderFlags_Logarithmic);
                        igSliderInt("Brush Radius", &bs->radius, 1, 10, "%d", ImGuiSliderFlags_AlwaysClamp);
                        // dropdown for possible BrushField targets
                        if (igBeginCombo("Data Layer", fieldName, 0)) {
                            // iterate through CellData members
                            ecs_iter_t it = ecs_term_iter(viewWorld, &(ecs_term_t){
                                .id = ecs_childof(ecs_id(CellData)),
                            });
                            while (ecs_term_next(&it)) {
                                for (int i = 0; i < it.count; i++) {
                                    ecs_entity_t ent = it.entities[i];
                                    const char *entName = getEntityLabel(viewWorld, ent);
                                    if (igSelectable_Bool(entName, ent == brushField, 0, IG_SIZE_DEFAULT)) {
                                        ecs_add_pair(viewWorld, ecs_id(AdditiveBrush), BrushField, ent);
                                    }
                                }
                            }
                            igEndCombo();
                        }

                        // Manual terrain smoothing
                        igSpacing();
                        static s32 smoothThreshold = 24;
                        static s32 smoothIterations = 30;
                        igText("Smoothing");
                        igSliderInt("##smoothThreshold", &smoothThreshold, 0, 100, "Threshold: %i", 0);
                        igSliderInt("##smoothIterations", &smoothThreshold, 1, 50, "Iterations: %i", 0);
                        if (igButton("Smooth map", IG_SIZE_DEFAULT)) {
                            const FocusPlane *fp = ecs_singleton_get(viewWorld, FocusPlane);
                            const Plane *plane = ecs_get(modelWorld, fp->entity, Plane);
                            for (int i = 0; i < smoothIterations; i++) {
                                bc_smoothTerrain(plane->chunkMap, smoothThreshold);
                            }
                            bc_redraw_model(viewWorld);
                        }

                        // TEST: Just copy the same values to the value brush instead of having separate settings
                        ecs_singleton_set(viewWorld, ValueBrush, {ab->value});
                        ecs_set(viewWorld, ecs_id(ValueBrush), BrushSize, {bs->radius});

                        ecs_singleton_modified(viewWorld, AdditiveBrush);
                        ecs_modified(viewWorld, ecs_id(AdditiveBrush), BrushSize);
                        igEndTabItem();
                    }
                    ecs_enable(viewWorld, terrainBindGroup, terrainBindActive);
                }
                if (riverBindGroup) {
                    bool riverBindActive = igBeginTabItem("Edit Rivers", NULL, ImGuiTabItemFlags_None);
                    if (riverBindActive) {
                        igText("Left click and drag to create a river connection between tiles");
                        igText("Right click and drag to remove river connections");

                        RiverBrush *rb = ecs_singleton_get_mut(viewWorld, RiverBrush);

                        if (igSliderInt("River Size", &rb->value, 1, 7, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                            ecs_singleton_modified(viewWorld, RiverBrush);
                        }

                        igEndTabItem();
                    }
                    ecs_enable(viewWorld, riverBindGroup, riverBindActive);
                }
                if (actorBindGroup) {
                    bool actorBindActive = igBeginTabItem("Create Actors", NULL, ImGuiTabItemFlags_None);
                    if (actorBindActive) {
                        igText("Left click to select tiles and entities");
                        igText("Right click to create an entity from selected prefab");
                        // Brush settings
                        PrefabBrush *pb = ecs_singleton_get_mut(viewWorld, PrefabBrush);
                        /* Ensure the model isn't running before doing anything else */
                        if (modelWorld) {
                            // set model ecs world scope, to keep view's external tags/queries separate
                            ecs_entity_t oldScope = ecs_get_scope(modelWorld);
                            ecs_entity_t viewScope = ecs_entity_init(modelWorld, &(ecs_entity_desc_t){.name = "bcview"});
                            ecs_set_scope(modelWorld, viewScope);

                            const char *prefabName = getEntityLabel(modelWorld, pb->prefab);
                            if (igBeginCombo("Select Prefab", prefabName, 0)) {
                                ecs_iter_t it = ecs_term_iter(modelWorld, &(ecs_term_t){
                                    .id = EcsPrefab,
                                    .flags = EcsTermMatchPrefab
                                });
                                // TODO: dedicated query for top-level prefabs
                                //ecs_iter_t it = ecs_query_iter(modelWorld, modelInspector.customQueries[2].query);
                                while (ecs_term_next(&it)) {
                                    for (int i = 0; i < it.count; i++) {
                                        ecs_entity_t ent = it.entities[i];
                                        const char *entName = getEntityLabel(modelWorld, ent);
                                        if (igSelectable_Bool(entName, ent == pb->prefab, 0, IG_SIZE_DEFAULT)) {
                                            modelInspector.focusEntity = ent;
                                            pb->prefab = ent;
                                        }
                                    }
                                }
                                igEndCombo();
                            }

                            if (pb->prefab) {
                                const char *brief = ecs_doc_get_brief(modelWorld, pb->prefab);
                                if (brief != NULL) {
                                    igTextWrapped(brief);
                                }
                                if (igCollapsingHeader_TreeNodeFlags("Edit Prefab", ImGuiTreeNodeFlags_None)) {
                                    // FIXME: cloning prefabs seems to crash flecs
                                    // if (igButton("Create new Prefab from this", IG_SIZE_DEFAULT)) {
                                    //     // copy to new prefab
                                    //     pb->prefab = ecs_clone(world, 0, pb->prefab, true);
                                    // }
                                    ecsEntityInspector(modelWorld, &modelInspector);
                                }
                            }

                            ecs_set_scope(modelWorld, oldScope);
                        } else {
                            igTextColored(IG_COLOR_WARNING, "Simulation currently running, press space to pause and edit prefabs");
                        }


                        ecs_singleton_modified(viewWorld, PrefabBrush);
                        igEndTabItem();
                    }
                    ecs_enable(viewWorld, actorBindGroup, actorBindActive);
                }
                if (playerBindGroup) {
                    bool playerBindActive = igBeginTabItem("Move Player", NULL, ImGuiTabItemFlags_None);
                    // to set visibility
                    TerrainPipelineUniformsVert *tv = ecs_get_mut(viewWorld, terrainPipeline, TerrainPipelineUniformsVert);
                    if (playerBindActive) {
                        igText("Left click to select tiles and entities");
                        igText("Right click to set player destination");
                        igText("Press space to advance time and move toward destination");
                        igText("Press c to focus camera on the player");

                        // Set visibility to 'play mode'
                        if (tv != NULL) tv->visibility = 0;

                        const PlayerEntity *player = ecs_singleton_get(viewWorld, PlayerEntity);
                        if (modelWorld) {
                            if (!ecs_is_valid(modelWorld, player->entity)) {
                                // assign player entity
                                ecs_iter_t it = ecs_term_iter(modelWorld, &(ecs_term_t){.id = ecs_id(MapVision)});
                                while(ecs_term_next(&it)) {
                                    if (it.count > 0) {
                                        ecs_singleton_set(viewWorld, PlayerEntity, {it.entities[0]});
                                        ecs_iter_fini(&it);
                                        break;
                                    }
                                }
                            }
                        }

                        igEndTabItem();
                    } else {
                        // Set visibility to 'edit mode'
                        if (tv != NULL) tv->visibility = 2;
                    }
                    ecs_modified(viewWorld, terrainPipeline, TerrainPipelineUniformsVert);
                    ecs_enable(viewWorld, playerBindGroup, playerBindActive);
                }

                igEndTabBar();
            }

        }
        igEnd();

        const FocusPlane *focusPlane = ecs_singleton_get(viewWorld, FocusPlane);
        if (modelWorld && focusPlane) {
            ecs_entity_t planeEntity = focusPlane->entity;
            if (planeEntity != 0) {
                // big inspector for selected cell
                // TODO: ensure this is in a lock
                const SelectedCell *selectedCell = ecs_singleton_get(viewWorld, SelectedCell);
                ecs_entity_t selectedRoot = plane_GetRootEntity(modelWorld, planeEntity, *selectedCell);
                if (selectedRoot != 0) {
                    const ImGuiWindowFlags flags = ImGuiWindowFlags_None;
                    if (igBegin("Selected Tile", NULL, flags)) {
                        relationshipTreeInspector(modelWorld, selectedRoot, IsIn, NULL, true);
                    }
                    igEnd();
                }

                // small preview for hovered cell
                const HoveredCell *hoveredCell = ecs_singleton_get(viewWorld, HoveredCell);
                ecs_entity_t hoveredRoot = plane_GetRootEntity(modelWorld, planeEntity, *hoveredCell);
                const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
                if (igBegin("##hoveredEntities", NULL, flags)) {
                    // lifezone name
                    const Plane *plane = ecs_get(modelWorld, planeEntity, Plane);
                    const Climate *climate = ecs_get(modelWorld, planeEntity, Climate);
                    if (climate) {
                        igText("%s", plane_getCellLifezoneName(plane, climate, *hoveredCell));
                    }

                    if (hoveredRoot != 0) {
                        const char *label = getEntityLabel(modelWorld, hoveredRoot);
                        // TODO: display either label or "multiple actors" if root is a CellRoot
                        igText("%s", label);
                    }
                }
                igEnd();
            }
        }

        if (modelWorld != NULL) {
            SDL_UnlockMutex(model->mutex);
        }
    }
}

void bc_editorOnModelStart(void) {
    modelInspector = (EcsInspectionContext){
        .worldName = "Model",
        .customQueries = {
            [0] = {.queryExpr = "Position, Plane(up(bc.planes.IsIn))"},
            [1] = {.queryExpr = "bc.planes.Plane"},
            [2] = {.queryExpr = "Prefab, !Prefab(up(ChildOf))"}, // only display top-level prefabs
            [3] = {.queryExpr = "flecs.system.System"},
        }
    };
}

void bc_editorOnModelStop(void) {
    modelInspector = (EcsInspectionContext){0};
}

void modelSetupInspector(bc_SupervisorInterface *si) {
    igText("World Generation Settings");

    igSliderInt("(in chunks)##chunkWidth", (int*)&ec.worldChunkWidth, 1, 16, "Width: %u", 0);
    igSliderInt("(in chunks)##chunkHeight", (int*)&ec.worldChunkHeight, 1, 16, "Height: %u", 0);
    if (ec.worldChunkWidth * ec.worldChunkHeight > 64) {
        igTextColored(IG_COLOR_WARNING, "Large worlds are currently unoptimized!", 0);
    }
    igText("Seed:");
    igInputText("##seedInput", ec.newGameSeed, 256, 0, NULL, NULL);

    if (igButton("Generate world", (ImVec2){0, 0})) {
        // TODO: need a new way to set model start settings; should probably just create singletons before running first model step
        // bc_ModelSetupSettings newSetupSettings = {
        //     .width = ec.worldChunkWidth,
        //     .height = ec.worldChunkHeight,
        //     .seed = ec.newGameSeed
        // };
        si->signal = BC_SUPERVISOR_SIGNAL_START_MODEL;
    }
}

// TODO: consider seperating the parts that require viewEcsWorld to another inspector section
void modelWorldInspector(ecs_world_t *modelWorld, ecs_world_t *viewWorld) {
    ecs_entity_t focusedPlane = ecs_singleton_get(viewWorld, FocusPlane)->entity;
    if (focusedPlane != 0) {
        htw_ChunkMap *cm = ecs_get(modelWorld, focusedPlane, Plane)->chunkMap;

        const HoveredCell *hovered = ecs_singleton_get(viewWorld, HoveredCell);
        const SelectedCell *selected = ecs_singleton_get(viewWorld, SelectedCell);

        // TODO: have a window attached to the mouse for hovered info, pin selected info here
        static bool inspectSelected = true;
        igCheckbox("Show Selected Cell Info", &inspectSelected);

        htw_geo_GridCoord focusCoord;
        if (inspectSelected) {
            focusCoord = *(htw_geo_GridCoord*)selected;
        } else {
            focusCoord = *(htw_geo_GridCoord*)hovered;
        }

        if (cellInspector(modelWorld, focusedPlane, focusCoord, &modelInspector.focusEntity)) {
            // Tell view to update chunk map
            bc_redraw_model(viewWorld);
        }
    }

    // Misc options
    if (igButton("Focus on selected cell", (ImVec2){0, 0})) {
        // TODO: modify this function to take Camera, ChunkMap, and GridCoord
        //bc_focusCameraOnCell(ui, selectedCellCoord);
    }
}

bool cellInspector(ecs_world_t *world, ecs_entity_t plane, htw_geo_GridCoord coord, ecs_entity_t *focusEntity) {
    const Plane *p = ecs_get(world, plane, Plane);
    const Climate *climate = ecs_get(world, plane, Climate);
    htw_ChunkMap *cm = p->chunkMap;
    bool edited = false;

    coordInspector("Cell coordinates", coord);

    if (igCollapsingHeader_TreeNodeFlags("Cell Data", 0)) {
        igSpacing();

        // TODO: should make some of this layout automatic based on CellData component reflection info, in case I change the types around
        CellData *cellData = htw_geo_getCell(cm, coord);
        igText("Raw Cell Info:");
        igPushItemWidth(200.0);
        //const s8 minHeight = INT8_MIN;
        //const s8 maxHeight = INT8_MAX;
        const s8 smallHeightStep = 1;
        const s8 bigHeightStep = 8;

        const u16 smallWaterStep = 8;
        const u16 bigWaterStep = 64;

        const u32 smallVegStep = 8;
        const u32 bigVegStep = 64;

        const u8 minVisibility = 0;
        const u8 maxVisibility = UINT8_MAX;
        edited |= igInputScalar("Height", ImGuiDataType_S8, &(cellData->height), &smallHeightStep, &bigHeightStep, NULL, 0);
        //edited |= igInputScalar("Geology", ImGuiDataType_U16, &(cellData->geology), &smallWaterStep, &bigWaterStep, NULL, 0);
        edited |= igInputScalar("Tracks", ImGuiDataType_U16, &(cellData->tracks), &smallWaterStep, &bigWaterStep, NULL, 0);
        igSameLine(0, -1);
        igText("%.1f%%", 100.0 * (float)cellData->tracks / (float)UINT16_MAX);
        edited |= igInputScalar("Groundwater", ImGuiDataType_S16, &(cellData->groundwater), &smallWaterStep, &bigWaterStep, NULL, 0);
        igSameLine(0, -1);
        if (cellData->groundwater < 0) {
            igText("days without water: %d", cellData->groundwater / -24);
        } else {
            igText("%.1f%%", 100.0 * (float)cellData->groundwater / (float)INT16_MAX);
        }
        edited |= igInputScalar("Surface Water", ImGuiDataType_U16, &(cellData->surfacewater), &smallWaterStep, &bigWaterStep, NULL, 0);
        igSameLine(0, -1);
        igText("%.1f%%", 100.0 * (float)cellData->surfacewater / (float)UINT16_MAX);
        edited |= igInputScalar("Humidity Preference", ImGuiDataType_U16, &(cellData->humidityPreference), &smallWaterStep, &bigWaterStep, NULL, 0);
        igSameLine(0, -1);
        igText("%.1f%%", 100.0 * (float)cellData->humidityPreference / (float)UINT16_MAX);
        edited |= igInputScalar("Understory", ImGuiDataType_U32, &(cellData->understory), &smallVegStep, &bigVegStep, NULL, 0);
        igSameLine(0, -1);
        igText("%.1f%%", 100.0 * (float)cellData->understory / (float)UINT32_MAX);
        edited |= igInputScalar("Canopy", ImGuiDataType_U32, &(cellData->canopy), &smallVegStep, &bigVegStep, NULL, 0);
        igSameLine(0, -1);
        igText("%.1f%%", 100.0 * (float)cellData->canopy / (float)UINT32_MAX);
        edited |= igSliderScalar("Visibility", ImGuiDataType_U8, &(cellData->visibility), &minVisibility, &maxVisibility, NULL, 0);
        igPopItemWidth();

        igText("Derived Cell Info:");
        s32 altitudeMeters = cellData->height * 100;
        igText("Altitude: %im", altitudeMeters);
        float temp = (float)plane_GetCellTemperature(p, climate, coord) / 100.0; // convert to celsius
        igText("Temperature: %.2f °C / %.2f °F", temp, temp * (9.0 / 5.0) + 32.0);
    }

    if (igCollapsingHeader_TreeNodeFlags("Cell Entities", ImGuiTreeNodeFlags_DefaultOpen)) {

        u32 entityCountHere = 0;
        ecs_entity_t selectedRoot = plane_GetRootEntity(world, plane, coord);
        if (selectedRoot != 0) {
            if (ecs_is_valid(world, selectedRoot)) {
                entityCountHere += relationshipTreeInspector(world, selectedRoot, IsIn, focusEntity, true);
            }
        }
        igValue_Uint("Entities here", entityCountHere);
    }

    return edited;
}

void ecsWorldInspector(ecs_world_t *world, EcsInspectionContext *ic) {
    igPushID_Ptr(world);

    ecs_defer_begin(world);

    // Menu bar for common actions
    if (igButton("New Entity", (ImVec2){0, 0})) {
        igOpenPopup_Str("create_entity", 0);
    }
    if (igBeginPopup("create_entity", 0)) {
        entityCreator(world, 0, &ic->focusEntity);
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

    ecs_defer_end(world);

    igPopID();
}

void ecsQueryColumns(ecs_world_t *world, EcsInspectionContext *ic) {
    ImVec2 avail;
    igGetContentRegionAvail(&avail);
    for (int i = 0; i < MAX_CUSTOM_QUERIES; i++) {
        igPushID_Int(i);
        if(igBeginChild_Str("Custom Query", (ImVec2){avail.x / MAX_CUSTOM_QUERIES, avail.y}, true, 0)) {
            ecsQueryInspector(world, &ic->customQueries[i], &ic->focusEntity);
        }
        igEndChild();
        igPopID();
        igSameLine(0, 0);
    }
}

void ecsQueryInspector(ecs_world_t *world, QueryContext *qc, ecs_entity_t *selected) {
    // TODO: could evaluate expression after every keypress, but only create query when enter is pressed. Would allow completion hints
    if (igIsWindowAppearing() || igInputText("Query", qc->queryExpr, MAX_QUERY_EXPR_LENGTH, ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL)) {
        qc->queryExprError[0] = '\0';
        if (qc->query != NULL) {
            ecs_query_fini(qc->query);
        }
        qc->query = createInspectorQuery(world, qc->queryExpr);
        if (qc->query == NULL) {
            // TODO: figure out if there is a way to get detailed expression error information from Flecs
            strcpy(qc->queryExprError, "Error: invalid query expression");
        }
        qc->queryPage = 0;
    }
    if (qc->queryExprError[0] != '\0') {
        igTextColored((ImVec4){1.0, 0.0, 0.0, 1.0}, qc->queryExprError);
    }

    ImGuiTextFilter_Draw(&qc->filter, "Filter", 0);

    igInputInt("Results per page", &qc->queryPageLength, 1, 1, 0);
    qc->queryPageLength = MAX(qc->queryPageLength, 10);

    if (qc->query != NULL) {
        ecs_iter_t it = ecs_query_iter(world, qc->query);
        entityList(world, &it, &qc->filter, (ImVec2){0.0, 0.0}, qc->queryPageLength, &qc->queryPage, selected);
    }
}

// TODO: should limit max number of displayed entities. For now, make sure bulk creation happens in some scope
void ecsTreeInspector(ecs_world_t *world, ecs_entity_t *selected) {
    //ecs_iter_t it = ecs_children(world, 0);
    ecs_iter_t it = ecs_term_iter(world, &(ecs_term_t){
        .id = ecs_childof(0),
        .flags = EcsTermMatchDisabled | EcsTermMatchPrefab
    });
    while (ecs_term_next(&it)) {
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

    // TODO: detail info for entity flags?
    // TODO: button to quick query for children of this entity
    // TODO: OR, auto expand selected entity in hierarchy

    // Editable name; must not set name if another entity in the same scope already has the same name
    if (entityButton(world, e)) {
        igOpenPopup_Str("edit_entity_name", 0);
    }
    igSetItemTooltip("Click to rename");
    if (igBeginPopup("edit_entity_name", 0)) {
        static char newName[256];
        if (igIsWindowAppearing()) {
            const char *currentName = getEntityLabel(world, e);
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

    // Shouldn't let any entity be deleted or disabled. Components, anything in the flecs namespace; systems should only be disabled, etc.
    bool disableAllowed = !ecs_has_id(world, e, EcsPrefab);
    bool deleteAllowed = disableAllowed &&
    !ecs_has_pair(world, e, EcsOnDelete, EcsPanic) &&
    !ecs_has_id(world, e, EcsSystem);

    bool enabled = !ecs_has_id(world, e, EcsDisabled);
    if (disableAllowed) {
        if (igCheckbox("Enabled", &enabled)) {
            ecs_enable(world, e, enabled);
        }
    }
    if (deleteAllowed) {
        igSameLine(0, -1);
        if (igSmallButton("Delete")) {
            ecs_delete(world, e);
            return; // Just in case operations aren't being deferred
        }
    }

    igBeginDisabled(!enabled);

    // Current Components, Tags, Relationships
    // TODO: also need to get components derived from IsA relationships
    const ecs_type_t *type = ecs_get_type(world, e);
    if (type != NULL) { // if entity has any components
        for (int i = 0; i < type->count; i++) {
            ecs_id_t id = type->array[i];
            igPushID_Int(id);
            // Pairs don't pass ecs_is_valid, so check for pair first
            if (ecs_id_is_pair(id)) {
                ecsPairWidget(world, e, id, &ic->focusEntity);
            } else if (ecs_is_valid(world, id)) {
                if (ecs_has(world, id, EcsComponent)) {
                    componentInspector(world, e, id, &ic->focusEntity);
                } else {
                    // TODO: dedicated tag inspector
                    if (entityButton(world, id)) {
                        ic->focusEntity = id;
                    }
                    igSameLine(0, -1);
                    if (igSmallButton("x")) {
                        ecs_remove_id(world, e, id);
                    }
                }
            } else {
                // Type may contain an invalid ID which is still meaningful (e.g. component override)
                // TODO: detect and handle specific cases like overrides
                igText("Unknown type: %u %x", id, id >> 32);
            }
            igPopID();
        }
    }
    igEndDisabled();

    igSpacing();

    // Add Component
    static ecs_entity_t newComponent = 0;
    if (igButton("Add Component", (ImVec2){0, 0})) {
        if (ic->components == NULL) {
            // create component query
            ic->components = createInspectorQuery(world, "EcsComponent");
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
            ic->tags = createInspectorQuery(world, "_");
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
            ic->relationships = createInspectorQuery(world, "EcsComponent || EcsTag || EcsFinal || EcsDontInherit || EcsAlwaysOverride || EcsTransitive || EcsReflexive || EcsAcyclic || EcsTraversable || EcsExclusive || EcsUnion || EcsSymmetric || EcsWith || EcsOneOf");
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

void ecsPairWidget(ecs_world_t *world, ecs_entity_t e, ecs_id_t pair, ecs_entity_t *focusEntity) {
    ecs_id_t first = ecs_pair_first(world, pair);
    if(!ecs_is_valid(world, first)) {
        ecs_err("Entity %s has pair with invalid relationship: %lu", ecs_get_name(world, e), first);
        return;
    }
    ecs_id_t second = ecs_pair_second(world, pair);
    if(!ecs_is_valid(world, second)) {
        ecs_err("Entity %s has pair with invalid target: %lu", ecs_get_name(world, e), second);
        return;
    }

    // TODO: if first == IsA, then could make this an expanding tree node instead, showing components inherited from the target

    igText("(");
    igSameLine(0, -1);
    if (entityButton(world, first)) {
        *focusEntity = first;
    }
    igSameLine(0, -1);
    if (entityButton(world, second)) {
        *focusEntity = second;
    }
    igSameLine(0, -1);
    igText(")");
    igSameLine(0, -1);
    if (igSmallButton("x")) {
        ecs_remove_id(world, e, pair);
        // must skip component inspector, otherwise pairs with data will get immediately re-added
        return;
    }
    if (ecs_has_id(world, first, EcsTag)) {
        // A tag relationship by definition contains no component data
        return;
    }

    // start meta inspectors on same line
    if (ecs_has(world, first, EcsMetaType)) {
        igSameLine(0, -1);
        void *componentData = ecs_get_mut_id(world, e, pair);
        ecs_meta_cursor_t cur = ecs_meta_cursor(world, first, componentData);
        ecsMetaInspector(world, &cur, focusEntity);
    } else if (ecs_has(world, second, EcsMetaType)) {
        igSameLine(0, -1);
        void *componentData = ecs_get_mut_id(world, e, pair);
        ecs_meta_cursor_t cur = ecs_meta_cursor(world, second, componentData);
        ecsMetaInspector(world, &cur, focusEntity);
    }
}

void entityCreator(ecs_world_t *world, ecs_entity_t parent, ecs_entity_t *focusEntity) {
    static char name[256];
    // Within a popup, clear name when opening
    if (igIsWindowAppearing()) {
        name[0] = '\0';
    }
    if (entityRenamer(world, parent, name, 256)) {
        *focusEntity = ecs_new_from_path(world, parent, name);
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
            // TODO: Inform user that there is already a sibling with the same name
            doesNameConflict = true;
        }
    }
    if (doesNameConflict) {
        igTextColored((ImVec4){1.0, 0.0, 0.0, 1.0}, "Name must be unique within scope");
    }
    return false;
}

u32 relationshipTreeInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t relationship, ecs_entity_t *focusRef, bool defaultOpen) {
    if (!ecs_is_valid(world, node)) {
        return 0;
    }
    ecs_entity_t focus = focusRef == NULL ? 0 : *focusRef;
    u32 count = 1;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    // Highlight focused node
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (node == focus) flags |= ImGuiTreeNodeFlags_Selected;

    bool hasChildren = false;
    ecs_iter_t children;
    // Attempting to search children for some Flecs builtins (., $) will cause an error, and others have no reason to display normally (_, *). Ignored list here is the same as the set checked in flecs_get_builtin
    if (node == EcsThis || node == EcsAny || node == EcsWildcard || node == EcsVariable) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    } else {
        children = ecs_term_iter(world, &(ecs_term_t){
            .id = ecs_pair(relationship, node),
            .flags = EcsTermMatchDisabled | EcsTermMatchPrefab
        });
        // Display as a leaf if no children
        if (ecs_term_next(&children)) {
            hasChildren = true;
        } else {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
    }

    const char *label = getEntityLabel(world, node);
    if (ecs_has_id(world, node, EcsDisabled)) {
        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_DISABLED);
    } else if (ecs_has_id(world, node, EcsModule)) {
        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_MODULE);
    } else if (ecs_has(world, node, EcsComponent)) {
        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_COMPONENT);
    } else if (ecs_has_id(world, node, EcsOneOf)) {
        // Acyclic, Traversable, Union, OneOf, and Tag all indicate that the entity is a relationship / pair first
        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_RELATIONSHIP);
    } else if (ecs_has_id(world, node, EcsPrefab)) {
        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_PREFAB);
    } else {
        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_ENTITY);
    }
    // Note: leaf nodes are always *expanded*, not collapsed. Should skip iterating children, but still TreePop if leaf.
    bool expandNode = igTreeNodeEx_Str(label, flags);
    igPopStyleColor(1);
    // If tree node was clicked, but not expanded, inspect node
    if (igIsItemClicked(ImGuiMouseButton_Left) && !igIsItemToggledOpen()) {
        focus = node;
    }

    // drag and drop
    if (igBeginDragDropSource(ImGuiDragDropFlags_None)) {
            igSetDragDropPayload("BC_PAYLOAD_ENTITY", &node, sizeof(ecs_entity_t), ImGuiCond_None);
            igEndDragDropSource();
    }
    if (igBeginDragDropTarget()) {
        const ImGuiPayload *payload = igAcceptDragDropPayload("BC_PAYLOAD_ENTITY", ImGuiDragDropFlags_None);
        if (payload != NULL) {
            ecs_entity_t payload_entity = *(const ecs_entity_t*)payload->Data;
            if (ecs_is_valid(world, payload_entity)) {
                // TODO: need to queue this action for end of frame?
                ecs_add_pair(world, payload_entity, EcsChildOf, node);
            }
        }
        igEndDragDropTarget();
    }

    if (expandNode) {
        if (hasChildren) {
            do {
                for (int i = 0; i < children.count; i++) {
                    count += hierarchyInspector(world, children.entities[i], &focus, defaultOpen);
                }
            } while (ecs_term_next(&children));
        }
        igTreePop();
    } else if (hasChildren) {
        ecs_iter_fini(&children);
    }
    if (focusRef != NULL) {
        *focusRef = focus;
    }
    return count;
}

u32 hierarchyInspector(ecs_world_t *world, ecs_entity_t node, ecs_entity_t *focus, bool defaultOpen) {
    return relationshipTreeInspector(world, node, EcsChildOf, focus, defaultOpen);
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
    static s32 page = 0;
    entityList(world, &it, &filter, (ImVec2){200.0, itemHeight * 10.0}, 10, &page, selected);

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
    //igBeginChild_Str("Entity List", (ImVec2){200.0, -1.0}, true, ImGuiWindowFlags_None);

    igText("Relationship");
    if (igIsWindowAppearing()) {
        igSetKeyboardFocusHere(0);
    }

    static ImGuiTextFilter relationshipFilter;
    ImGuiTextFilter_Draw(&relationshipFilter, "##", 0);
    ecs_iter_t relationshipIter = ecs_query_iter(world, relationshipQuery);
    static s32 relationshipPage = 0;
    entityList(world, &relationshipIter, &relationshipFilter, (ImVec2){200.0, itemHeight * 10.0}, 10, &relationshipPage, &relationship);
    //igEndChild();
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

    igText("Target");
    static ImGuiTextFilter targetFilter;
    ImGuiTextFilter_Draw(&targetFilter, "##", 0);
    static s32 targetPage = 0;
    entityList(world, &targetIter, &targetFilter, (ImVec2){200.0, itemHeight * 10.0}, 10, &targetPage, &target);
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

bool entityList(ecs_world_t *world, ecs_iter_t *it, ImGuiTextFilter *filter, ImVec2 size, s32 pageLength, s32 *pageNumber, ecs_entity_t *selected) {
    bool anyClicked = false;
    //igBeginChild_Str("List Window", size, true, ImGuiWindowFlags_None);
    float height = igGetTextLineHeightWithSpacing() * (pageLength + 1);
    igBeginChild_Str("Entity List", (ImVec2){size.x, height}, true, ImGuiWindowFlags_HorizontalScrollbar);
    u32 resultsCount = 0;
    *pageNumber = MAX(1, *pageNumber);
    u32 firstResult = pageLength * (*pageNumber - 1);
    u32 lastResult = firstResult + pageLength;

    while (ecs_iter_next(it)) {
        // Fields from default expression: ?Module, ?Component, ?Tag, ?Prefab, ?Disabled
        bool isModule = ecs_field_is_set(it, 1);
        bool isComponent = ecs_field_is_set(it, 2);
        bool isTag = ecs_field_is_set(it, 3);
        bool isPrefab = ecs_field_is_set(it, 4);
        bool isDisabled = ecs_field_is_set(it, 5);

        for (int i = 0; i < it->count; i++) {
            ecs_entity_t e = it->entities[i];
            const char *name = getEntityLabel(world, e);
            // TODO: make filter optional?
            if (ImGuiTextFilter_PassFilter(filter, name, NULL)) {
                if (resultsCount >= firstResult && resultsCount < lastResult) {
                    igPushID_Int(e);
                    if (isDisabled) {
                        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_DISABLED);
                    } else if (isModule) {
                        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_MODULE);
                    } else if (isComponent || isTag) {
                        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_COMPONENT);
                    }
                    // TODO: add a way to check for relationship types with the default query
                    // else if (isRelationship) {
                    //     igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_RELATIONSHIP);
                    // }
                    else if (isPrefab) {
                        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_PREFAB);
                    } else {
                        igPushStyleColor_Vec4(ImGuiCol_Text, IG_COLOR_ENTITY);
                    }
                    // NOTE: igSelectable will, by default, call CloseCurrentPopup when clicked. Set flag to disable this behavior
                    if (igSelectable_Bool(name, e == *selected, ImGuiSelectableFlags_DontClosePopups, (ImVec2){0, 0})) {
                        *selected = e;
                        anyClicked = true;
                    }
                    igPopStyleColor(1);
                    // drag and drop
                    if (igBeginDragDropSource(ImGuiDragDropFlags_None)) {
                        igSetDragDropPayload("BC_PAYLOAD_ENTITY", &e, sizeof(ecs_entity_t), ImGuiCond_None);
                        igEndDragDropSource();
                    }
                    // TODO: should be its own function
                    if (igBeginPopupContextItem("##context_menu", ImGuiPopupFlags_MouseButtonRight)) {
                        if (igSelectable_Bool("Create Instance", false, 0, (ImVec2){0, 0})) {
                            *selected = bc_instantiateRandomizer(world, e);
                        }
                        // TODO: more context options
                        igEndPopup();
                    }
                    igPopID();
                }
                resultsCount++;
            }
        }
    }
    igEndChild(); // Entity List
    // TODO: after converting to table, add blank rows so other controls don't change position
    igValue_Uint("Results", resultsCount);
    s32 pageCount = ((resultsCount - 1) / pageLength) + 1;
    if (pageCount > 1) {
        if (igArrowButton("page_left", ImGuiDir_Left)) (*pageNumber)--;
        igSameLine(0, -1);
        if (igArrowButton("page_right", ImGuiDir_Right)) (*pageNumber)++;
        igSameLine(0, -1);
        igSliderInt("Page", pageNumber, 1, pageCount, NULL, ImGuiSliderFlags_AlwaysClamp);
    }
    *pageNumber = CLAMP(*pageNumber, 1, pageCount);
    //igEndChild(); // List Window
    
    return anyClicked;

    // TODO: make page number optional, display "x more results..." if not provided
    // if (resultsCount > pageLength) {
    //     igTextColored((ImVec4){0.5, 0.5, 0.5, 1.0}, "%i more results...", resultsCount - pageLength);
    // }
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
    igSameLine(0, -1);

    bool modified = false;
    // custom inspectors for specific component types
    khint_t i = kh_get(CustomInspector, customInspectors, component);
    if (i != kh_end(customInspectors)) {
        bc_componentInspector inspectorCallback = kh_val(customInspectors, i);
        modified = inspectorCallback(world, e, component);
    } else if (ecs_has(world, component, EcsMetaType)) {
        // NOTE: calling ecs_get_mut_id AFTER a call to ecs_remove_id will end up adding the removed component again, with default values
        void *componentData = ecs_get_mut_id(world, e, component);
        ecs_meta_cursor_t cur = ecs_meta_cursor(world, component, componentData);
        modified = ecsMetaInspector(world, &cur, focusEntity);
    } else {
        // component with no meta info and no custom inspector
        igTextColored(IG_COLOR_DISABLED, "(opaque component)");
    }

    if (modified) {
        ecs_modified_id(world, e, component);
    }

    igEndGroup();
}

bool ecsMetaInspector(ecs_world_t *world, ecs_meta_cursor_t *cursor, ecs_entity_t *focusEntity)
{
    // FIXME: should have a general-purpose way to disallow editor modification of some component types, especially flecs internal stuff e.g. MetaType
    bool modified = false;
    int i = 0;
    do {
        igPushID_Int(i);
        ecs_entity_t type = ecs_meta_get_type(cursor);
        if (type != 0) {
            const char *memberName = ecs_meta_get_member(cursor);
            if (memberName != NULL) {
                igText("%s:", memberName);
                igSetItemTooltip("%s", ecs_get_name(world, type));
            }
            ecs_type_kind_t kind = ecs_get(world, type, EcsMetaType)->kind;
            switch (kind) {
                case EcsPrimitiveType:
                    igSameLine(0, -1);
                    ecs_primitive_kind_t primKind = ecs_get(world, type, EcsPrimitive)->kind;
                    modified |= primitiveInspector(world, primKind, ecs_meta_get_ptr(cursor), focusEntity);
                    break;
                case EcsStructType:
                    // recurse componentInspector
                    // should indent + newline for struct, array, vector, (and bitmask?) types
                    igNewLine();
                    igIndent(0);
                    ecs_meta_push(cursor);
                    // TODO: component members with special componentInspector handling should also get their custom inspectors here
                    modified |= ecsMetaInspector(world, cursor, focusEntity);
                    ecs_meta_pop(cursor);
                    igUnindent(0);
                    break;
                case EcsEnumType:
                    igSameLine(0, -1);
                    ecs_map_t enumMap = ecs_get(world, type, EcsEnum)->constants;
                    s64 key = ecs_meta_get_int(cursor);
                    ecs_enum_constant_t *enumDesc = *ecs_map_get_ref(&enumMap, ecs_enum_constant_t, key);
                    //igText("%s = %i", enumDesc->name, enumDesc->value);
                    if (igBeginCombo("##enumList", enumDesc->name, 0)) {
                        // FIXME: map iter is out of order from enum order; better to just do an ecs_children iter?
                        ecs_map_iter_t mi = ecs_map_iter(&enumMap);
                        while (ecs_map_next(&mi)) {
                            s64 key2 = *mi.res;
                            ecs_enum_constant_t *enumEntry = *ecs_map_get_ref(&enumMap, ecs_enum_constant_t, key2);
                            if (igSelectable_Bool(enumEntry->name, key == key2, 0, IG_SIZE_DEFAULT)) {
                                s32 *val = ecs_meta_get_ptr(cursor);
                                *val = enumEntry->value;
                                modified = true;
                            }
                        }
                        igEndCombo();
                    }
                    break;
                default:
                    // TODO priority order: bitmask, array, vector, opaque
                    igSameLine(0, -1);
                    igText("Unhandled type kind");
                    break;
            }
        }
        igPopID();
        i++;
    } while (ecs_meta_try_next(cursor));

    return modified;
}

bool primitiveInspector(ecs_world_t *world, ecs_primitive_kind_t kind, void *field, ecs_entity_t *focusEntity) {
    bool modified = false;
    switch (kind) {
        case EcsBool:
            modified = igCheckbox("##", field);
            break;
        case EcsChar:
            // NOTE: char is s8 on some platforms. Not currently using any components with single char members, so unsure if this matters much
            modified = igInputScalar("##", ImGuiDataType_U8, field, NULL, NULL, "%c", 0);
            break;
        case EcsByte:
            modified = igInputScalar("##", ImGuiDataType_U8, field, NULL, NULL, "%x", 0);
            break;
        case EcsU8:
            modified = igInputScalar("##", ImGuiDataType_U8, field, NULL, NULL, NULL, 0);
            break;
        case EcsU16:
            modified = igInputScalar("##", ImGuiDataType_U16, field, NULL, NULL, NULL, 0);
            break;
        case EcsU32:
            modified = igInputScalar("##", ImGuiDataType_U32, field, NULL, NULL, NULL, 0);
            break;
        case EcsU64:
            modified = igInputScalar("##", ImGuiDataType_U64, field, NULL, NULL, NULL, 0);
            break;
        case EcsI8:
            modified = igInputScalar("##", ImGuiDataType_S8, field, NULL, NULL, NULL, 0);
            break;
        case EcsI16:
            modified = igInputScalar("##", ImGuiDataType_S16, field, NULL, NULL, NULL, 0);
            break;
        case EcsI32:
            modified = igInputInt("##", field, 1, 10, 0);
            break;
        case EcsI64:
            modified = igInputScalar("##", ImGuiDataType_S32, field, NULL, NULL, NULL, 0);
            break;
        case EcsF32:
            modified = igDragFloat("##", field, 0.02f, -FLT_MAX, FLT_MAX, "%.3f", 0);
            break;
        case EcsF64:
            modified = igDragScalar("##", ImGuiDataType_Double, field, 0.02f, NULL, NULL, "%.6g", 0);
            break;
        case EcsUPtr:
            modified = igInputScalar("##", ImGuiDataType_U64, field, NULL, NULL, "%x", 0);
            break;
        case EcsIPtr:
            modified = igInputScalar("##", ImGuiDataType_S64, field, NULL, NULL, "%x", 0);
            break;
        case EcsString:
            (void)0; // So kdevelop's parser doesn't freak out /shrug
            char *str = *(char**)field;
            //igText(str);
            //u64 len = strlen(str);
            //igTextUnformatted(str, str + len);
            // display as normal string until edit button clicked
            if (str != NULL) {
                if(igSmallButton("edit")) {
                    strncpy(stringEditBuffer, str, STRING_BUFFER_SIZE);
                    igOpenPopup_Str("edit_string", 0);
                }
                igSameLine(0, -1);
                igTextWrapped(str);
                // begin edit window for string
                if (igBeginPopup("edit_string", 0)) {
                    // inputText should get default focus
                    if (igIsWindowAppearing()) {
                        igSetKeyboardFocusHere(0);
                    }
                    // TODO: any way to make imgui's text input wrap single line text?
                    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine;
                    if(igInputTextMultiline("##edit_this_string", stringEditBuffer, STRING_BUFFER_SIZE, (ImVec2){600, 0}, flags, NULL, NULL)) {
                        size_t len = strlen(stringEditBuffer) + 1; // strlen doesn't include terminator
                        char *newStr = malloc(len);
                        strncpy(newStr, stringEditBuffer, len);
                        free(str);
                        *(char**)field = newStr;
                        modified = true;
                        igCloseCurrentPopup();
                    }
                    igEndPopup();
                }
            } else {
                if(igSmallButton("set")) {
                    stringEditBuffer[0] = '\0';
                    igOpenPopup_Str("set_string", 0);
                }
                // begin edit window for string
                if (igBeginPopup("set_string", 0)) {
                    // inputText should get default focus
                    if (igIsWindowAppearing()) {
                        igSetKeyboardFocusHere(0);
                    }
                    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine;
                    if(igInputTextMultiline("##edit_this_string", stringEditBuffer, STRING_BUFFER_SIZE, (ImVec2){600, 0}, flags, NULL, NULL)) {
                        size_t len = strlen(stringEditBuffer) + 1; // strlen doesn't include terminator
                        char *newStr = malloc(len);
                        strncpy(newStr, stringEditBuffer, len);
                        *(char**)field = newStr;
                        modified = true;
                        igCloseCurrentPopup();
                    }
                    igEndPopup();
                }
            }
            break;
        case EcsEntity:
        {
            ecs_entity_t e = *(ecs_entity_t*)(field);
            // FIXME: doesn't account for entities from other ECS worlds. Maybe create a special component type + inspector for displaying world entities?
            if (entityButton(world, e)) {
                *focusEntity = e;
            }

            // dragdrop assignable
            if (igBeginDragDropTarget()) {
                const ImGuiPayload *payload = igAcceptDragDropPayload("BC_PAYLOAD_ENTITY", ImGuiDragDropFlags_None);
                if (payload != NULL) {
                    memcpy(field, payload->Data, sizeof(ecs_entity_t));
                    modified = true;
                }
                igEndDragDropTarget();
            }
            break;
        }
        default:
            igText("Error: unhandled primitive type");
            break;
    }
    return modified;
}

const char *getEntityLabel(ecs_world_t *world, ecs_entity_t e) {
    static char idStr[64];
    if (e == 0) {
        return "(nothing)";
    } else if (!ecs_is_alive(world, e)) {
        sprintf(idStr, "non-alive entity %u", ecs_entity_t_lo(e));
        return idStr;
    } else if (ecs_is_valid(world, e)) {
        const char *name = ecs_get_name(world, e);
        if (name == NULL) {
            sprintf(idStr, "%u", ecs_entity_t_lo(e));
            return idStr;
        } else {
            return name;
        }
    } else {
        sprintf(idStr, "INVALID ENTITY %lu", e);
        return idStr;
    }
}

void entityLabel(ecs_world_t *world, ecs_entity_t e) {
    igText("%s", getEntityLabel(world, e));

    // Consider enabling this for extra info
    // if (igIsItemHovered(0)) {
    //     char *tooltip = ecs_get_fullpath(world, e);
    //     igSetTooltip(tooltip);
    //     ecs_os_free(tooltip);
    // }
}

// TODO: recolor based on entity type?
bool entityButton(ecs_world_t *world, ecs_entity_t e) {
    const char *label = getEntityLabel(world, e);
    igBeginDisabled(!ecs_is_valid(world, e));
    bool pressed = igButton(label, IG_SIZE_DEFAULT);
    // imgui doesn't allow dragging from disabled elements, so only valid entities can be picked from here
    if (igBeginDragDropSource(ImGuiDragDropFlags_None)) {
        igSetDragDropPayload("BC_PAYLOAD_ENTITY", &e, sizeof(ecs_entity_t), ImGuiCond_None);
        igText(label);
        igEndDragDropSource();
    }
    igEndDisabled();
    return pressed;
}



void possessEntity(ecs_world_t *world, ecs_entity_t target) {
    // TODO: ensure only one entity has mapvision at a time
    // TODO: focus camera on target
    // TODO: set PlayerEntity singleton in view world
    if (ecs_is_valid(world, target)) {
        ecs_add_pair(world, target, Ego, EgoNone);
        ecs_set(world, target, MapVision, {.range = 3});
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

    // probably don't need to fixed width format specifiers, but handy to know how to use them
    igText("%" PRIu64 "/%" PRIu64 "/%" PRIu64 " Hour %" PRIu64, year, month, day, hour);
}

void coordInspector(const char *label, htw_geo_GridCoord coord) {
    igText(label);
    igValue_Int("X", coord.x);
    igSameLine(igGetCursorPosX() + 64.0, -1);
    igValue_Int("Y", coord.y);
}

void renderTargetInspector(ecs_world_t *world) {
    // FIXME: neither method of getting a TextureID for the shadow map works, always causes OpenGL errors when presenting
    // static simgui_image_t spImage = {0};
    // if (spImage.id == 0) {
    //     spImage = simgui_make_image(&(simgui_image_desc_t){
    //         .image = sp->image,
    //         //.sampler = sp->sampler // Might need different sampler
    //     });
    // }
    // ImTextureID texID = simgui_imtextureid(spImage);

    //u32 gluint = bc_sg_getImageGluint(sp->image);
    //ImTextureID texID = &gluint;

    ImGuiIO *io = igGetIO();
    ImVec4 white = { 1, 1, 1, 1 };
    igImage(io->Fonts->TexID, (ImVec2){512, 512}, (ImVec2){0.0, 0.0}, (ImVec2){1.0, 1.0}, white, white);
}

void toolPaletteInspector(ecs_world_t *world) {
    // List tools
    // Keep track of selected tool
    // Mini entity inspector for editing tool values?
    // Assign selected tool to ActiveTool (how?)
}

bool queryDescriptionInspector(ecs_world_t *world, ecs_entity_t e, ecs_entity_t component) {
    bool modified = false;
    //QueryDesc *qd = ecs_get_mut(world, e, QueryDesc);
    // button opens input for new query expression, will create description from expression
    if (igButton("Set Query Expression", (ImVec2){0, 0})) {
        igOpenPopup_Str("query_expr", 0);
    }
    if (igBeginPopup("query_expr", 0)) {
        QueryDesc *qd = ecs_get_mut(world, e, QueryDesc);
        static char queryExpr[MAX_QUERY_EXPR_LENGTH];
        if (igIsWindowAppearing()) {
            if (qd->expr == NULL) {
                // TODO
            } else {
                memcpy(queryExpr, qd->expr, MAX_QUERY_EXPR_LENGTH);
            }
        }
        if (igInputText("Query", queryExpr, MAX_QUERY_EXPR_LENGTH, ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL)) {
            // TODO: ensure null handled
            memcpy(qd->expr, queryExpr, MAX_QUERY_EXPR_LENGTH);
            modified = true;
            igCloseCurrentPopup();
        }
        igEndPopup();
    }
    return modified;
}

bool colorInspector(ecs_world_t *world, ecs_entity_t e, ecs_entity_t component) {
    // Color picker
    static ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_PickerHueWheel;
    Color *col = ecs_get_mut(world, e, Color);
    return igColorEdit4("##color_picker", col->v, flags);
}

ecs_query_t *createInspectorQuery(ecs_world_t *world, const char *expr) {
    // NOTE: Flecs explorer's entity tree includes these terms in every query, to alter entity display by property
    // Extra term not supported in cached queries, means singleton: ?ChildOf(_, $This)
    const char *defaultExpr = "?Module, ?Component, ?Tag, ?Prefab, ?Disabled, ";
    // Merge default and user query expressions together, with default terms first
    char fullExpr[sizeof(defaultExpr) + MAX_QUERY_EXPR_LENGTH];
    strcpy(fullExpr, defaultExpr);
    strcat(fullExpr, expr);
    return ecs_query_init(world, &(ecs_query_desc_t){
        .filter.expr = fullExpr
    });
}
