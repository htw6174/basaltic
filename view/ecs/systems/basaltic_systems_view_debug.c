#include "basaltic_systems_view_debug.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
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

    size_t instanceDataSize = sizeof(DebugInstanceData) * DEBUG_INSTANCE_COUNT;
    // TODO: create this per entity. Make component destructor to free memory
    DebugInstanceData *instanceData = malloc(instanceDataSize);

    sg_buffer_desc vbd = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_DYNAMIC,
        .size = instanceDataSize
    };

    sg_bindings bind = {
        .vertex_buffers[0] = sg_make_buffer(&vbd)
    };

    free(vert);
    free(frag);

    for (int i = 0; i < it->count; i++) {
        ecs_set(it->world, it->entities[i], Pipeline, {pip});
        ecs_set(it->world, it->entities[i], Binding, {.binding = bind, .instances = DEBUG_INSTANCE_COUNT});
        ecs_set(it->world, it->entities[i], QueryDesc, {
            .desc.filter.terms = {
                {.id = ecs_id(Position), .inout = EcsIn},
                {.id = ecs_pair(IsOn, EcsAny), .inout = EcsIn}
            }
        });
        ecs_set(it->world, it->entities[i], InstanceBuffer, {.size = instanceDataSize, .data = instanceData});
    }
}

void UpdateDebugPipelineBuffers(ecs_iter_t *it) {
    Binding *binds = ecs_field(it, Binding, 1);
    ModelQuery *queries = ecs_field(it, ModelQuery, 2);
    InstanceBuffer *instanceBuffers = ecs_field(it, InstanceBuffer, 3);

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
        binds[i].instances = instanceCount;
        sg_update_buffer(binds[i].binding.vertex_buffers[0], &(sg_range){.ptr = instanceData, .size = instanceBuffers[i].size});
    }
}

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
}

void BasalticSystemsViewDebugImport(ecs_world_t *world) {
    ECS_MODULE(world, BasalticSystemsViewDebug);

    ECS_IMPORT(world, BasalticComponentsView);
    ECS_IMPORT(world, BasalticPhasesView);

    ECS_SYSTEM(world, InitDebugPipeline, EcsOnStart,
        [out] !Pipeline,
        [out] !Binding,
        [out] !QueryDesc,
        [none] basaltic.components.view.DebugRender
    );

    ECS_SYSTEM(world, UpdateDebugPipelineBuffers, OnModelChanged,
        [in] Binding,
        [in] ModelQuery,
        [out] InstanceBuffer,
        [in] Scale($),
        [in] ModelWorld($),
        [none] basaltic.components.view.DebugRender
    )

    ECS_SYSTEM(world, DrawDebugPipeline, EcsOnUpdate,
        [in] Pipeline,
        [in] Binding,
        [in] PVMatrix($),
        [none] ModelWorld($),
        [none] WrapInstanceOffsets($),
        [none] basaltic.components.view.DebugRender,
    );

    ecs_entity_t debugDraw = ecs_new_id(world);
    ecs_add(world, debugDraw, DebugRender);
}
