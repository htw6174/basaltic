#include <sys/stat.h>

#define BC_COMPONENT_IMPL
#include "bc_components_common.h"

// Breaking the rules here by defining a system within a component module; later on should figure out a better way to give 2 different ECS worlds access to the same common systems
void ReloadFlecsScript(ecs_iter_t *it) {
    ResourceFile *flecsScriptSources = ecs_field(it, ResourceFile, 1);

    for (int i = 0; i < it->count; i++) {
        u64 lastAccess = flecsScriptSources[i].accessTime;

        struct stat fileStat;
        if (stat(flecsScriptSources[i].path, &fileStat) != 0) {
            // TODO: handle file read error
            continue;
        }
        time_t modifyTime = fileStat.st_mtime;

        if (modifyTime > lastAccess) {
            flecsScriptSources[i].accessTime = flecsScriptSources[i].accessTime = time(NULL);
            int err = ecs_plecs_from_file(it->world, flecsScriptSources[i].path);
            if (err != 0) {
                // TODO: catch file opening or parsing errors, handle/display log message
            }
        }
    }
}

void BcCommonImport(ecs_world_t *world) {
    ECS_MODULE(world, BcCommon);

    // Creating meta info for custom primitive types:
    // 1. Declare and Define a component for the type
    // 2. Use ecs_primitive to create meta info
    ECS_COMPONENT_DEFINE(world, s8);
    ECS_COMPONENT_DEFINE(world, s16);
    ECS_COMPONENT_DEFINE(world, s32);
    ECS_COMPONENT_DEFINE(world, s64);

    ECS_COMPONENT_DEFINE(world, time_t);

    ecs_primitive(world, {.entity = ecs_id(s8), .kind = EcsI8});
    ecs_primitive(world, {.entity = ecs_id(s16), .kind = EcsI16});
    ecs_primitive(world, {.entity = ecs_id(s32), .kind = EcsI32});
    ecs_primitive(world, {.entity = ecs_id(s64), .kind = EcsI64});

    ecs_primitive(world, {.entity = ecs_id(time_t), .kind = EcsU64});

    ECS_META_COMPONENT(world, ResourceFile);

    ECS_TAG_DEFINE(world, FlecsScriptSource);

    ECS_SYSTEM(world, ReloadFlecsScript, EcsOnUpdate,
        [inout] (ResourceFile, bc.common.FlecsScriptSource)
    );
    // NOTE: no_readonly is required as the flecs parser needs access to the actual world
    ecs_system(world, {
        .entity = ReloadFlecsScript,
        .no_readonly = true
    });
}
