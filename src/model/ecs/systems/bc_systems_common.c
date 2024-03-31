#include "bc_systems_common.h"
#include "bc_components_common.h"
#include "basaltic_components_planes.h"
#include "basaltic_worldGen.h"
#include "htw_random.h"
#include <sys/stat.h>

typedef struct {
    u32 chunkSize;
    u32 width;
    u32 height;
    char *seed;
} worldStartSettings;

void argsToStartSettings(int argc, char *argv[], worldStartSettings *outSettings) {
    // assign defaults
    outSettings->seed = "6174";
    outSettings->width = 3;
    outSettings->height = 3;
    // TODO: make chunk size configurable with arg
    outSettings->chunkSize = 64;

    // TODO: have this change settings according to name provided in arg; for now will use no names and expect a specific order
    for (int i = 0; i < argc; i++) {
        switch (i) {
            case 0:
                outSettings->seed = argv[i];
                break;
            case 1:
                outSettings->width = htw_strToInt(argv[i]);
                break;
            case 2:
                outSettings->height = htw_strToInt(argv[i]);
                break;
            default:
                break;
        }
    }
}

void ParseArgs(ecs_iter_t *it) {
    Args *args = ecs_field(it, Args, 1);

    worldStartSettings startSettings;
    argsToStartSettings(args->argc, args->argv, &startSettings);

    // TODO: create additional singletons from start settings

    u32 seed = xxh_hash(0, 256, (u8*)startSettings.seed);

    // Create default terrain
    htw_ChunkMap *cm = bc_createTerrain(startSettings.chunkSize, startSettings.width, startSettings.height);
    bc_generateTerrain(cm, seed);
    ecs_entity_t centralPlane = ecs_set(it->world, 0, Plane, {cm});
    ecs_set_name(it->world, centralPlane, "Overworld");
}

void ReloadPlecs(ecs_iter_t *it) {
    ResourceFile *flecsScriptSources = ecs_field(it, ResourceFile, 1);

    ecs_defer_suspend(it->world);
    for (int i = 0; i < it->count; i++) {

        time_t lastAccess = flecsScriptSources[i].accessTime;

        struct stat fileStat;
        if (stat(flecsScriptSources[i].path, &fileStat) != 0) {
            // handle file access error
            ecs_err("Failed reloading flecs script for entity %s: Could not access resource file \"%s\"", ecs_get_name(it->world, it->entities[i]), flecsScriptSources[i].path);
            // Disable to prevent repeatedly trying an invalid path
            ecs_enable(it->world, it->entities[i], false);
            continue;
        }
        time_t modifyTime = fileStat.st_mtime;

        if (modifyTime > lastAccess) {
            flecsScriptSources[i].accessTime = flecsScriptSources[i].accessTime = time(NULL);
            int err = ecs_plecs_from_file(it->world, flecsScriptSources[i].path);
            if (err != 0) {
                ecs_err("Failed reading flecs script %s", flecsScriptSources[i].path);
                // TODO: catch specific file opening or parsing errors, handle/display log message
            }
        }
    }
    ecs_defer_resume(it->world);
}

void IncrementStep(ecs_iter_t *it) {
    Step *step = ecs_field(it, Step, 1);
    (*step)++;
}

void BcSystemsCommonImport(ecs_world_t *world) {

    ECS_MODULE(world, BcSystemsCommon);

    ECS_IMPORT(world, BcCommon);

    ECS_SYSTEM(world, ParseArgs, EcsOnStart,
        [in] Args($)
    );

    // TODO does this do everything I need? Can I remove the equivalent from bc_flecs_utils?
    ECS_SYSTEM(world, ReloadPlecs, EcsOnLoad,
        [inout] (ResourceFile, bc.common.FlecsScriptSource)
    );
    ecs_system(world, {
        .entity = ReloadPlecs,
        .no_readonly = true
    });
    // Limit refresh and reload to once per second
    ecs_entity_t reloadTick = ecs_set_interval(world, 0, 1.0);
    ecs_set_tick_source(world, ReloadPlecs, reloadTick);

    ECS_SYSTEM(world, IncrementStep, EcsOnStore,
        Step($)
    );

    ecs_singleton_set(world, Step, {0});
}
