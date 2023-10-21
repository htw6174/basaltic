#ifndef BC_COMPONENTS_COMMON_H_INCLUDED
#define BC_COMPONENTS_COMMON_H_INCLUDED

#include "htw_core.h"
#include "flecs.h"
#include "time.h"

#undef ECS_META_IMPL
#undef BC_DECL
#ifndef BC_COMPONENT_IMPL
#define ECS_META_IMPL EXTERN
#define BC_DECL extern
#else
#define BC_DECL
#endif

// Primitive types
// NOTE: u* and i* prefix int types already have meta defs setup by Flecs; I just define s* prefix int types to match my typing system
BC_DECL ECS_COMPONENT_DECLARE(s8);
BC_DECL ECS_COMPONENT_DECLARE(s16);
BC_DECL ECS_COMPONENT_DECLARE(s32);
BC_DECL ECS_COMPONENT_DECLARE(s64);

typedef u64 Step;
BC_DECL ECS_COMPONENT_DECLARE(Step);
BC_DECL ECS_COMPONENT_DECLARE(time_t);

ECS_STRUCT(ResourceFile, {
    // update whenever file is read; if modify time on disk is > this, should reload file
    time_t accessTime;
    char path[256];
});

// Target of a ResourceFile relationship
BC_DECL ECS_TAG_DECLARE(FlecsScriptSource);

void BcCommonImport(ecs_world_t *world);

#endif // BC_COMPONENTS_COMMON_H_INCLUDED
