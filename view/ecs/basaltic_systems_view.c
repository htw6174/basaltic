#include "basaltic_systems_view.h"
#include "basaltic_components_view.h"
#include "systems/basaltic_systems_view_terrain.h"
#include "systems/basaltic_systems_view_debug.h"
#include "htw_core.h"
#include "sokol_gfx.h"

// Unused; maybe have a more general input->uniform system?
void UniformPointerToMouse(ecs_iter_t *it) {

}

void UniformCameraToPVMatrix(ecs_iter_t *it) {
    const Camera *cam = ecs_singleton_get(it->world, Camera);
    const WindowSize *w = ecs_singleton_get(it->world, WindowSize);
    PVMatrix *pv = ecs_singleton_get_mut(it->world, PVMatrix);

    // TODO: figure out if inner loops should be used for singleton-only systems
    //for (int i = 0; i < it->count; i++) {
    {
        mat4x4 p, v;
        vec4 cameraPosition;
        vec4 cameraFocalPoint;

        float pitch = cam->pitch * DEG_TO_RAD;
        float yaw = cam->yaw * DEG_TO_RAD;
        float correctedDistance = powf(cam->distance, 2.0);
        float radius = cos(pitch) * correctedDistance;
        float height = sin(pitch) * correctedDistance;
        float floor = cam->origin.z; //ui->activeLayer == BC_WORLD_LAYER_SURFACE ? 0 : -8;
        cameraPosition = (vec4){
            .x = cam->origin.x + radius * sin(yaw),
            .y = cam->origin.y + radius * -cos(yaw),
            .z = floor + height
        };
        cameraFocalPoint = (vec4){
            .x = cam->origin.x,
            .y = cam->origin.y,
            .z = floor
        };

        mat4x4Perspective(p, PI / 3.f, (float)w->x / w->y, 0.1f, 10000.f);
        mat4x4LookAt(v,
            cameraPosition.xyz,
            cameraFocalPoint.xyz,
            (vec3){ {0.f, 0.f, 1.f} }
        );

        mat4x4MultiplyMatrix(pv->pv, v, p);
    }

    ecs_singleton_modified(it->world, PVMatrix);
}

void CreateModelQueries(ecs_iter_t *it) {
    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;
    QueryDesc *terms = ecs_field(it, QueryDesc, 1);

    for (int i = 0; i < it->count; i++) {
        ecs_query_t *query = ecs_query_init(modelWorld, &(terms[i].desc));
        ecs_set(it->world, it->entities[i], ModelQuery, {query});
    }
}

void DestroyModelQueries(ecs_iter_t *it) {
    for (int i = 0; i < it->count; i++) {
        ecs_remove(it->world, it->entities[i], ModelQuery);
    }
}

void BasalticSystemsViewImport(ecs_world_t *world) {
    ECS_MODULE(world, BasalticSystemsView);

    ECS_IMPORT(world, BasalticComponentsView);
    ECS_IMPORT(world, BasalticSystemsViewDebug);
    ECS_IMPORT(world, BasalticSystemsViewTerrain);

    // char *path = ecs_get_fullpath(world, TerrainRender);
    // printf("%s\n", path);
    // assert(ecs_lookup_fullpath(world, path) == ecs_id(TerrainRender));
    // ecs_os_free(path);

    ECS_SYSTEM(world, UniformCameraToPVMatrix, EcsPreUpdate,
        [in] Camera($),
        [in] WindowSize($),
        [out] PVMatrix($)
    );

    // After attaching to model, create queries in the model's ecs world
    ECS_SYSTEM(world, CreateModelQueries, EcsOnLoad,
        [in] QueryDesc,
        [in] ModelWorld($),
        [out] !ModelQuery
    );

    // Remove queries after model is detached TODO: do this before detatching, so that queries can be cleaned up on the model?
    ECS_SYSTEM(world, DestroyModelQueries, EcsOnLoad,
        [out] ModelQuery,
        [none] !ModelWorld($)
    );

    // A system with no query will run once per frame. However, an end of frame call is already being handled by the core engine. Something like this might be useful for starting and ending individual render passes
    //ECS_SYSTEM(world, SokolCommit, EcsOnStore, 0);
}
