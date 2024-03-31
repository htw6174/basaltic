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

// Instance field randomizers
ECS_ENUM(RandomizerDistribution, {
    RAND_DISTRIBUTION_UNIFORM,
    RAND_DISTRIBUTION_EXPONENTIAL,
    RAND_DISTRIBUTION_NORMAL
});

// Command line arguments
ECS_STRUCT(Args, {
    s32 argc;
    void *argv;
});

/**
 * @brief RandomizerInt
 * Relationship where the target is a component field of a matching type
 * Behavior of each field depends on the random distribution used
 * @member min: min possible value; normal = 3 stdev below the mean
 * @member max: max possible value; normal = 3 stdev above the mean
 * @member mean: uniform = n/a; exponential = mean (can place outside [low, high] to flatten out the distribution) ; normal = peak of bell curve
 */
ECS_STRUCT(RandomizeInt, {
    s64 min;
    s64 max;
    s64 mean;
    RandomizerDistribution distribution;
});

ECS_STRUCT(RandomizeFloat, {
    double min;
    double max;
    double mean;
    RandomizerDistribution distribution;
});

ECS_STRUCT(ResourceFile, {
    // update whenever file is read; if modify time on disk is > this, should reload file
    time_t accessTime;
    char *path;
});

// Target of a ResourceFile relationship
BC_DECL ECS_TAG_DECLARE(FlecsScriptSource);

void BcCommonImport(ecs_world_t *world);

#endif // BC_COMPONENTS_COMMON_H_INCLUDED
