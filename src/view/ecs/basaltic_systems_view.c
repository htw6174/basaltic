#include "basaltic_systems_view.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "systems/basaltic_systems_view_terrain.h"
#include "systems/basaltic_systems_view_debug.h"
#include "htw_core.h"
#include "sokol_gfx.h"
#include "basaltic_sokol_gfx.h"
#include <sys/stat.h>

void UniformPointerToMouse(ecs_iter_t *it) {
    Pointer *pointers = ecs_field(it, Pointer, 1);
    const WindowSize *w = ecs_singleton_get(it->world, WindowSize);

    // TODO: need any special handling for multiple pointer inputs? For now just override with last pointer
    for (int i = 0; i < it->count; i++) {
        float x = pointers[i].x + 0.5;
        float y = (w->y - pointers[i].y) + 0.5;
        ecs_singleton_set(it->world, Mouse, {x, y});
    }
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

void SetupPipelines(ecs_iter_t *it) {
    ResourceFile *vertSources = ecs_field(it, ResourceFile, 1);
    ResourceFile *fragSources = ecs_field(it, ResourceFile, 2);
    PipelineDescription *pipelineDescriptions = ecs_field(it, PipelineDescription, 3);
    Pipeline *pipelines = ecs_field(it, Pipeline, 4);

    for (int i = 0; i < it->count; i++) {
        vertSources[i].accessTime = fragSources[i].accessTime = time(NULL);
        PipelineDescription pd = pipelineDescriptions[i];
        Pipeline p;
        int err = bc_loadShader(vertSources[i].path, fragSources[i].path, pd.shader_desc, &p.shader);
        err |= bc_createPipeline(pd.pipeline_desc, &p.shader, &p.pipeline);
        if (err == 0) {
            ecs_set_id(it->world, it->entities[i], ecs_id(Pipeline), sizeof(p), &p);
        }
    }
}

void ReloadPipelines(ecs_iter_t *it) {
    ResourceFile *vertSources = ecs_field(it, ResourceFile, 1);
    ResourceFile *fragSources = ecs_field(it, ResourceFile, 2);
    PipelineDescription *pipelineDescriptions = ecs_field(it, PipelineDescription, 3);
    Pipeline *pipelines = ecs_field(it, Pipeline, 4);

    for (int i = 0; i < it->count; i++) {
        time_t vLastAccess = vertSources[i].accessTime;
        time_t fLastAccess = fragSources[i].accessTime;

        struct stat fileStat;
        int err = stat(vertSources[i].path, &fileStat);
        if (err != 0) continue;
        time_t vModifyTime = fileStat.st_mtime;

        err = stat(fragSources[i].path, &fileStat);
        if (err != 0) continue;
        time_t fModifyTime = fileStat.st_mtime;

        if (vModifyTime > vLastAccess || fModifyTime > fLastAccess) {
            vertSources[i].accessTime = fragSources[i].accessTime = time(NULL);
            PipelineDescription pd = pipelineDescriptions[i];
            Pipeline p;
            int err = bc_loadShader(vertSources[i].path, fragSources[i].path, pd.shader_desc, &p.shader);
            err |= bc_createPipeline(pd.pipeline_desc, &p.shader, &p.pipeline);
            if (err == 0) {
                sg_destroy_pipeline(pipelines[i].pipeline);
                sg_destroy_shader(pipelines[i].shader);

                pipelines[i] = p;
            } else {
                // TODO: find a good way to represent that pipeline couldn't be updated
            }
        }
    }
}

void CreateModelQueries(ecs_iter_t *it) {
    QueryDesc *terms = ecs_field(it, QueryDesc, 1);

    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;

    for (int i = 0; i < it->count; i++) {
        terms[i].desc.filter.expr = terms[i].expr;
        ecs_query_t *query = ecs_query_init(modelWorld, &(terms[i].desc));
        ecs_set(it->world, it->entities[i], ModelQuery, {query});
    }
}

void CreateSubQueries(ecs_iter_t *it) {
    QueryDesc *terms = ecs_field(it, QueryDesc, 1);
    ModelQuery *baseQueries = ecs_field(it, ModelQuery, 2);
    //ecs_id_t renderPipeline = ecs_field_id(it, 2);
    //const ModelQuery *existingQuery = ecs_get(it->world, ECS_PAIR_SECOND(renderPipeline), ModelQuery);

    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;

    for (int i = 0; i < it->count; i++) {
        terms[i].desc.parent = baseQueries[i].query;
        terms[i].desc.filter.expr = terms[i].expr;
        ecs_query_t *query = ecs_query_init(modelWorld, &(terms[i].desc));
        if (query != NULL) {
            ecs_set(it->world, it->entities[i], ModelQuery, {query});
        }
    }
}

void RemoveModelQueries(ecs_iter_t *it) {
    for (int i = 0; i < it->count; i++) {
        ecs_remove(it->world, it->entities[i], ModelQuery);
    }
}

void DestroyModelQueries(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);
    for (int i = 0; i < it->count; i++) {
        ecs_query_fini(queries[i].query);
    }
}

void DrawInstances(ecs_iter_t *it) {
    Pipeline *pipelines = ecs_field(it, Pipeline, 1);
    InstanceBuffer *ibs = ecs_field(it, InstanceBuffer, 2);
    Elements *eles = ecs_field(it, Elements, 3);
    PVMatrix *pvs = ecs_field(it, PVMatrix, 4);

    const WrapInstanceOffsets *wraps = ecs_singleton_get(it->world, WrapInstanceOffsets);

    for (int i = 0; i < it->count; i++) {
        sg_bindings bind = {
            .vertex_buffers[0] = ibs[0].buffer
        };

        sg_apply_pipeline(pipelines[i].pipeline);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));

        bc_drawWrapInstances(0, eles[i].count, ibs[i].instances, 1, (vec3){{0.0, 0.0, 0.0}}, wraps->offsets);
    }
}

void BcviewSystemsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewSystems);

    ECS_IMPORT(world, Bcview);
    ECS_IMPORT(world, BcviewPhases);
    ECS_IMPORT(world, BcviewSystemsDebug);
    ECS_IMPORT(world, BcviewSystemsTerrain);

    // char *path = ecs_get_fullpath(world, TerrainRender);
    // printf("%s\n", path);
    // assert(ecs_lookup_fullpath(world, path) == ecs_id(TerrainRender));
    // ecs_os_free(path);

    ECS_SYSTEM(world, UniformPointerToMouse, EcsPreUpdate,
        [in] Pointer,
        [in] WindowSize($),
        [out] Mouse($)
    );

    ECS_SYSTEM(world, UniformCameraToPVMatrix, EcsPreUpdate,
        [in] Camera($),
        [in] WindowSize($),
        [out] PVMatrix($)
    );

    ECS_SYSTEM(world, SetupPipelines, EcsOnUpdate,
        [inout] (ResourceFile, bcview.VertexShaderSource),
        [inout] (ResourceFile, bcview.FragmentShaderSource),
        [in] PipelineDescription,
        [out] !Pipeline,
    );

    // TODO: allow setting a limited update rate for this system
    ECS_SYSTEM(world, ReloadPipelines, EcsOnUpdate,
        [inout] (ResourceFile, bcview.VertexShaderSource),
        [inout] (ResourceFile, bcview.FragmentShaderSource),
        [in] PipelineDescription,
        [out] Pipeline,
    );

    //ecs_enable(world, ReloadPipelines, false); // TODO: Should enable/disable by default based on build type

    ECS_SYSTEM(world, CreateModelQueries, EcsOnLoad,
        [in] QueryDesc,
        [in] ModelWorld($),
        [out] !ModelQuery,
    );

    // Must create subqueries after creating parent queries, Flecs' scheduler *should* handle this automatically because of the [out] ModelQuery above and [in] ModelQuery term here
    ECS_SYSTEM(world, CreateSubQueries, EcsOnLoad,
        [in] QueryDesc,
        [in] ModelQuery(parent),
        [in] ModelWorld($),
        [out] !ModelQuery,
    );

    // Remove query references after model is detached, the queries themselves are cleaned up automatically by Flecs
    ECS_SYSTEM(world, RemoveModelQueries, EcsOnLoad,
        [out] ModelQuery,
        [none] !ModelWorld($)
    );

    // Call fini on Model queries only while Model world still exists
    ECS_OBSERVER(world, DestroyModelQueries, EcsOnRemove,
        ModelQuery,
        [none] ModelWorld($)
    );

    // TODO: experiment with GroupBy to cluster draws with the same pipeline and/or bindings
    ECS_SYSTEM(world, DrawInstances, EcsOnUpdate,
               [in] Pipeline(up(bcview.RenderPipeline)), // NOTE: the source specifier "up(RenderPipeline)" gets component from target of RenderPipeline relationship for $this
               [in] InstanceBuffer,
               [in] Elements,
               [in] PVMatrix($),
               [none] WrapInstanceOffsets($),
    );

    // A system with no query will run once per frame. However, an end of frame call is already being handled by the core engine. Something like this might be useful for starting and ending individual render passes
    //ECS_SYSTEM(world, SokolCommit, EcsOnStore, 0);
}
