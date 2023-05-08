#include "basaltic_systems_view_debug.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "basaltic_components.h"
#include "components/basaltic_components_planes.h"
#include "ccVector.h"
#include "sokol_gfx.h"
#include "basaltic_sokol_gfx.h"

const size_t DEBUG_INSTANCE_COUNT = 1024;

typedef struct {
    vec4 position;
    vec4 color;
    float scale;
} DebugInstanceData;

void InitDebugPipeline(ecs_iter_t *it) {
    char *vert = htw_load("view/shaders/debug.vert");
    char *frag = htw_load("view/shaders/debug.frag");

    // TODO: better way to setup uniform block descriptions? Maybe add description as component to each uniform component, or use sokol's shader reflection to autogen
    sg_shader_uniform_block_desc vsdGlobal = {
        .size = sizeof(PVMatrix),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "pv", .type = SG_UNIFORMTYPE_MAT4},
        }
    };

    sg_shader_uniform_block_desc vsdLocal = {
        .size = sizeof(ModelMatrix),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "m", .type = SG_UNIFORMTYPE_MAT4},
        }
    };

    sg_shader_desc tsd = {
        .vs.uniform_blocks[0] = vsdGlobal,
        .vs.uniform_blocks[1] = vsdLocal,
        .vs.source = vert,
        .fs.source = frag
    };

    sg_pipeline_desc pd = {
        .shader = sg_make_shader(&tsd),
        .layout = {
            .buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT4,
                [1].format = SG_VERTEXFORMAT_FLOAT4,
                [2].format = SG_VERTEXFORMAT_FLOAT,
            }
        },
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_NONE
    };
    sg_pipeline pip = sg_make_pipeline(&pd);

    free(vert);
    free(frag);

    for (int i = 0; i < it->count; i++) {
        ecs_add_pair(it->world, it->entities[i], EcsChildOf, RenderPipeline);
        ecs_set(it->world, it->entities[i], Pipeline, {pip});
        ecs_set(it->world, it->entities[i], QueryDesc, {
            .desc.filter.terms = {
                {.id = ecs_id(Position), .inout = EcsIn},
                {.id = ecs_pair(IsOn, EcsAny), .inout = EcsIn}
            }
        });
    }
}

// TODO: can generalize this by requiring instance size and count
void InitDebugBuffers(ecs_iter_t *it) {
    size_t instanceDataSize = sizeof(DebugInstanceData) * DEBUG_INSTANCE_COUNT;

    for (int i = 0; i < it->count; i++) {
        // TODO: Make component destructor to free memory
        DebugInstanceData *instanceData = malloc(instanceDataSize);

        sg_buffer_desc vbd = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
            .size = instanceDataSize
        };
        sg_buffer instanceBuffer = sg_make_buffer(&vbd);

        //ecs_set(it->world, it->entities[i], Binding, {.binding = bind, .elements = 24, .instances = DEBUG_INSTANCE_COUNT});
        ecs_set(it->world, it->entities[i], InstanceBuffer, {.buffer = instanceBuffer, .size = instanceDataSize, .data = instanceData});
    }

}

void UpdateDebugBuffers(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);
    InstanceBuffer *instanceBuffers = ecs_field(it, InstanceBuffer, 2);

    const vec3 *scale = ecs_singleton_get(it->world, Scale);
    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;

    for (int i = 0; i < it->count; i++) {
        DebugInstanceData *instanceData = instanceBuffers[i].data;
        u32 instanceCount = 0;

        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Position *positions = ecs_field(&mit, Position, 1);
            ecs_entity_t tmEnt = ecs_field_id(&mit, 2);
            htw_ChunkMap *cm = ecs_get(mit.world, ecs_pair_second(mit.world, tmEnt), Plane)->chunkMap;
            for (int m = 0; m < mit.count; m++) {
                u32 chunkIndex, cellIndex;
                htw_geo_gridCoordinateToChunkAndCellIndex(cm, positions[m], &chunkIndex, &cellIndex);
                s32 elevation = bc_getCellByIndex(cm, chunkIndex, cellIndex)->height;
                float posX, posY;
                htw_geo_getHexCellPositionSkewed(positions[m], &posX, &posY);
                instanceData[instanceCount] = (DebugInstanceData){
                    .position = {.xyz = vec3MultiplyVector((vec3){{posX, posY, elevation}}, *scale), .__w = 1.0},
                    .color = {{1.0, 0.0, 1.0, 1.0}},
                    .scale = 1.0
                };
                instanceCount++;
            }
        }
        instanceBuffers[i].instances = instanceCount;
        sg_update_buffer(instanceBuffers[i].buffer, &(sg_range){.ptr = instanceData, .size = instanceBuffers[i].size});
    }
}

/*
void DrawDebugPipeline(ecs_iter_t *it) {
    Pipeline *pips = ecs_field(it, Pipeline, 1);
    Binding *binds = ecs_field(it, Binding, 2);
    PVMatrix *pvs = ecs_field(it, PVMatrix, 3);

    const WrapInstanceOffsets *wraps = ecs_singleton_get(it->world, WrapInstanceOffsets);

    for (int i = 0; i < it->count; i++) {
        sg_apply_pipeline(pips[i].pipeline);
        sg_apply_bindings(&binds[i].binding);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));

        bc_drawWrapInstances(0, 24, binds[i].instances, 1, (vec3){{0.0, 0.0, 0.0}}, wraps->offsets);
    }
}*/

void DrawInstances(ecs_iter_t *it) {
    ecs_id_t renderPipeline = ecs_field_id(it, 1);
    InstanceBuffer *ibs = ecs_field(it, InstanceBuffer, 2);
    Elements *eles = ecs_field(it, Elements, 3);
    PVMatrix *pvs = ecs_field(it, PVMatrix, 4);

    const WrapInstanceOffsets *wraps = ecs_singleton_get(it->world, WrapInstanceOffsets);

    const Pipeline *pipeline = ecs_get(it->world, ECS_PAIR_SECOND(renderPipeline), Pipeline);

    for (int i = 0; i < it->count; i++) {
        sg_bindings bind = {
            .vertex_buffers[0] = ibs[0].buffer
        };

        sg_apply_pipeline(pipeline->pipeline);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));

        bc_drawWrapInstances(0, eles[i].count, ibs[i].instances, 1, (vec3){{0.0, 0.0, 0.0}}, wraps->offsets);
    }
}

void BasalticSystemsViewDebugImport(ecs_world_t *world) {
    ECS_MODULE(world, BasalticSystemsViewDebug);

    // FIXME: need to be more careful about imports. Ran into an issue where a Model world query constructed here would match nothing, unless this import is done
    ECS_IMPORT(world, BasalticComponentsPlanes);
    ECS_IMPORT(world, BasalticComponentsView);
    ECS_IMPORT(world, BasalticPhasesView);

    // ECS_SYSTEM(world, InitDebugPipeline, EcsOnStart,
    //     [out] !Pipeline,
    //     [out] !QueryDesc,
    //     [none] basaltic.components.view.DebugRender
    // );

    ECS_OBSERVER(world, InitDebugPipeline, EcsOnAdd,
        [out] Pipeline,
        [out] !QueryDesc,
        [none] basaltic.components.view.DebugRender
    );

    ECS_OBSERVER(world, InitDebugBuffers, EcsOnAdd,
        [out] InstanceBuffer,
        [none] basaltic.components.view.DebugRender
    );

    ECS_SYSTEM(world, UpdateDebugBuffers, OnModelChanged,
        [in] ModelQuery,
        [inout] InstanceBuffer,
        [in] Scale($),
        [in] ModelWorld($),
        [none] basaltic.components.view.DebugRender
    )

    // ECS_SYSTEM(world, DrawDebugPipeline, EcsOnUpdate,
    //     [in] Pipeline,
    //     [in] Binding,
    //     [in] PVMatrix($),
    //     [none] ModelWorld($),
    //     [none] WrapInstanceOffsets($),
    //     [none] basaltic.components.view.DebugRender,
    // );

    // TODO: use query variables to get the Pipeline component directly. For now, just check for the relationship and try to get the component
    ECS_SYSTEM(world, DrawInstances, EcsOnUpdate,
        [in] (basaltic.components.view.RenderPipeline, _),
        [in] InstanceBuffer,
        [in] Elements,
        [in] PVMatrix($),
        [none] ModelWorld($),
        [none] WrapInstanceOffsets($),
    );

    // Always override, so instance's model query can become a subquery of the original
    //ecs_add_id(world, ecs_id(ModelQuery), EcsAlwaysOverride);
    // Test: instead use DontInherit, so the CreateModelQueries system picks up subqueries
    //ecs_add_id(world, ecs_id(ModelQuery), EcsDontInherit);
    //ecs_add_id(world, ecs_id(QueryDesc), EcsDontInherit);

    // Pipeline, only need to create one per type
    ecs_entity_t debugPipeline = ecs_set_name(world, 0, "DebugPipeline");
    ecs_add_id(world, debugPipeline, DebugRender);
    ecs_add(world, debugPipeline, Pipeline);
    // OnAdd observer initializes Pipeline and adds QueryDesc, and makes the entity a child of RenderPipeline

    // Instance buffer, need to create one for every set of instances to be rendered, as well as a subquery used to fill that buffer
    // aka "Draw Query"
    ecs_entity_t debugInstancesDefault = ecs_set_name(world, 0, "DebugInstancesDefault");
    ecs_add_id(world, debugInstancesDefault, DebugRender);
    ecs_add_pair(world, debugInstancesDefault, RenderPipeline, debugPipeline);
    ecs_add(world, debugInstancesDefault, InstanceBuffer);
    ecs_set(world, debugInstancesDefault, Elements, {24});
    // TODO: add behavior to make debugInstancesDefault(ModelQuery) a subquery of the original value, with this description
    ecs_set(world, debugInstancesDefault, QueryDesc, {
        .desc.filter.terms = {
            {.id = ecs_id(Position), .inout = EcsIn},
            {.id = ecs_pair(IsOn, EcsAny), .inout = EcsIn},
            {.id = ecs_id(PlayerVision), .inout = EcsIn} // NOTE: even if the header is included, if the correct module is not imported then entity IDs may default to 0, preventing them from becoming usable filter terms. The query builder Macros can help to detect this at runtime, but having a compile time check would be very helpful
        }
    });
}
