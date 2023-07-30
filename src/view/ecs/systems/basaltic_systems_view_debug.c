#include "basaltic_systems_view_debug.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "basaltic_components.h"
#include "components/basaltic_components_planes.h"
#include "ccVector.h"
#include "sokol_gfx.h"
#include "basaltic_sokol_gfx.h"

const size_t DEBUG_INSTANCE_COUNT = 1024;

// TODO: make a more general instance data struct. This style of instanced rendering will be used for most draw queries. Can add to this slightly for more versatility (single axis rotation etc.), and add consistancy to the fields required for filling instance buffers (Position, (IsOn _))
typedef struct {
    vec4 position;
    vec4 color;
    float scale;
} DebugInstanceData;

static sg_shader_desc debugShaderDescription = {
    .vs.uniform_blocks[0] = {
        .size = sizeof(PVMatrix),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "pv", .type = SG_UNIFORMTYPE_MAT4},
        }
    },
    .vs.uniform_blocks[1] = {
        .size = sizeof(ModelMatrix),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "m", .type = SG_UNIFORMTYPE_MAT4},
        }
    },
    .vs.source = NULL,
    .fs.source = NULL
};

static sg_pipeline_desc debugPipelineDescription = {
    .shader = {0},
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

        ecs_set(it->world, it->entities[i], InstanceBuffer, {.maxInstances = DEBUG_INSTANCE_COUNT, .instances = 0, .buffer = instanceBuffer, .size = instanceDataSize, .data = instanceData});
        ecs_set(it->world, it->entities[i], Elements, {24});
    }

}

void UpdateDebugBuffers(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);
    InstanceBuffer *instanceBuffers = ecs_field(it, InstanceBuffer, 2);
    // Optional term, may be null
    Color *colors = ecs_field(it, Color, 3);

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
            for (int m = 0; m < mit.count && instanceCount < instanceBuffers[i].maxInstances; m++) {
                u32 chunkIndex, cellIndex;
                htw_geo_gridCoordinateToChunkAndCellIndex(cm, positions[m], &chunkIndex, &cellIndex);
                s32 elevation = bc_getCellByIndex(cm, chunkIndex, cellIndex)->height;
                float posX, posY;
                htw_geo_getHexCellPositionSkewed(positions[m], &posX, &posY);
                instanceData[instanceCount] = (DebugInstanceData){
                    .position = {.xyz = vec3MultiplyVector((vec3){{posX, posY, elevation}}, *scale), .__w = 1.0},
                    .color = colors == NULL ? (vec4){{1.0, 0.0, 1.0, 1.0}} : colors[i],
                    .scale = 1.0
                };
                instanceCount++;
            }
        }
        instanceBuffers[i].instances = instanceCount;
        sg_update_buffer(instanceBuffers[i].buffer, &(sg_range){.ptr = instanceData, .size = instanceBuffers[i].size});
    }
}

void BcviewSystemsDebugImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewSystemsDebug);

    ECS_IMPORT(world, Bcview);
    ECS_IMPORT(world, BcviewPhases);

    ECS_OBSERVER(world, InitDebugBuffers, EcsOnAdd,
        [out] InstanceBuffer,
        [out] ?Elements,
        [none] bcview.DebugRender
    );

    ECS_SYSTEM(world, UpdateDebugBuffers, OnModelChanged,
        [in] ModelQuery,
        [inout] InstanceBuffer,
        [in] ?Color,
        [in] Scale($),
        [in] ModelWorld($),
        [none] bcview.DebugRender
    )

    // Always override, so instance's model query can become a subquery of the original
    //ecs_add_id(world, ecs_id(ModelQuery), EcsAlwaysOverride);
    // Test: instead use DontInherit, so the CreateModelQueries system picks up subqueries
    //ecs_add_id(world, ecs_id(ModelQuery), EcsDontInherit);
    //ecs_add_id(world, ecs_id(QueryDesc), EcsDontInherit);

    // Pipeline, only need to create one per type
    ecs_entity_t debugPipeline = ecs_set_name(world, 0, "DebugPipeline");
    ecs_add_pair(world, debugPipeline, EcsChildOf, RenderPipeline);
    ecs_set_pair(world, debugPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/debug.vert"});
    ecs_set_pair(world, debugPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/debug.frag"});
    ecs_set(world, debugPipeline, PipelineDescription, {.shader_desc = &debugShaderDescription, .pipeline_desc = &debugPipelineDescription});

    // Instance buffer, need to create one for every set of instances to be rendered, as well as a subquery used to fill that buffer
    // aka "Draw Query"
    ecs_entity_t debugPrefab = ecs_set_name(world, 0, "DebugDrawPrefab");
    ecs_add_id(world, debugPrefab, EcsPrefab);
    ecs_add_id(world, debugPrefab, DebugRender);
    //ecs_add_pair(world, debugInstancesDefault, RenderPipeline, debugPipeline);
    // NOTE/FIXME: subqueries have their own fields, separate from the parent query. Need to add terms required by UpdateDebugBuffers here, in the right order, or find another solution. Possibility: could store the start of a term list or query expr, and add the draw queries' terms to that. Possibility: don't use fields when iterating the query, just get components that are known to be in the parent query. Should measure perf difference with this approach.
    ecs_set(world, debugPrefab, QueryDesc, {
        // .desc.filter.terms = {
        //     {.id = ecs_id(Position), .inout = EcsIn}, // NOTE: even if the header is included, if the correct module is not imported then entity IDs may default to 0, preventing them from becoming usable filter terms. The query builder Macros can help to detect this at runtime, but having a compile time check would be very helpful
        //     {.id = ecs_pair(IsOn, EcsAny), .inout = EcsIn}
        // }
        .expr = "Position, (bc.planes.IsOn, _)"
    });
    ecs_set(world, debugPrefab, Color, {{1.0, 0.0, 1.0, 1.0}});
    ecs_override_pair(world, debugPrefab, RenderPipeline, debugPipeline);
    ecs_override(world, debugPrefab, InstanceBuffer);
    ecs_override(world, debugPrefab, QueryDesc);
    ecs_override(world, debugPrefab, Color);

    // Prefab instance
    // TEST: different colors for each diet type
    ecs_entity_t meatVis = ecs_new_w_pair(world, EcsIsA, debugPrefab);
    ecs_set_name(world, meatVis, "DrawQuery diet meat");
    ecs_set(world, meatVis, Color, {{0.7, 0.0, 0.0, 1.0}});
    ecs_set(world, meatVis, QueryDesc, {
        .expr = "Position, (bc.planes.IsOn, _), (bc.wildlife.Diet, bc.wildlife.Diet.Meat)"
    });

    ecs_entity_t fruitVis = ecs_new_w_pair(world, EcsIsA, debugPrefab);
    ecs_set_name(world, fruitVis, "DrawQuery diet fruit");
    ecs_set(world, fruitVis, Color, {{0.7, 0.0, 0.7, 1.0}});
    ecs_set(world, fruitVis, QueryDesc, {
        .expr = "Position, (bc.planes.IsOn, _), (bc.wildlife.Diet, bc.wildlife.Diet.Fruit)"
    });

    ecs_entity_t foliageVis = ecs_new_w_pair(world, EcsIsA, debugPrefab);
    ecs_set_name(world, foliageVis, "DrawQuery diet foliage");
    ecs_set(world, foliageVis, Color, {{0.0, 0.7, 0.7, 1.0}});
    ecs_set(world, foliageVis, QueryDesc, {
        .expr = "Position, (bc.planes.IsOn, _), (bc.wildlife.Diet, bc.wildlife.Diet.Foliage)"
    });

    ecs_entity_t grassVis = ecs_new_w_pair(world, EcsIsA, debugPrefab);
    ecs_set_name(world, grassVis, "DrawQuery diet grass");
    ecs_set(world, grassVis, Color, {{0.0, 0.7, 0.0, 1.0}});
    ecs_set(world, grassVis, QueryDesc, {
        .expr = "Position, (bc.planes.IsOn, _), (bc.wildlife.Diet, bc.wildlife.Diet.Grasses)"
    });

    //ecs_enable(world, ecs_id(DrawInstances), false);
}