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

static sg_shader_desc debugShadowShaderDescription = {
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

static sg_pipeline_desc debugShadowPipelineDescription = {
    .shader = {0},
    .layout = {
        .buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
        .buffers[0].stride = sizeof(DebugInstanceData),
        .attrs = {
            [0].format = SG_VERTEXFORMAT_FLOAT4,
        }
    },
    .depth = {
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true,
        .bias = 0.01,
        .bias_slope_scale = 1.0
    },
    .cull_mode = SG_CULLMODE_BACK,
    .sample_count = 1,
    // important: 'deactivate' the default color target for 'depth-only-rendering'
    .colors[0].pixel_format = SG_PIXELFORMAT_NONE
};

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
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true
    },
    .cull_mode = SG_CULLMODE_BACK,
    .color_count = 3,
    .colors = {
        [0].pixel_format = SG_PIXELFORMAT_RGBA8,
        [1].pixel_format = SG_PIXELFORMAT_RGBA16F,
        [2].pixel_format = SG_PIXELFORMAT_R32F,
    }
};

static sg_shader_desc debugRTShaderDescription = {
    .fs.images[0] = {
        .used = true,
        .image_type = SG_IMAGETYPE_2D,
        .sample_type = SG_IMAGESAMPLETYPE_DEPTH
    },
    .fs.samplers[0] = {
        .used = true,
        .sampler_type = SG_SAMPLERTYPE_FILTERING
    },
    .fs.image_sampler_pairs[0] = {
        .used = true,
        .image_slot = 0,
        .sampler_slot = 0,
        .glsl_name = "renderTarget"
    },
};

static sg_pipeline_desc debugRTPipelineDescription = {
    .layout = {
        .attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
    },
    .depth.pixel_format = SG_PIXELFORMAT_NONE,
    .shader = {0},
    .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    .label = "debug-RT-pipeline",
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

void DrawRenderTargetPreviews(ecs_iter_t *it) {
    Pipeline *pipelines = ecs_field(it, Pipeline, 1);
    Mesh *meshes = ecs_field(it, Mesh, 2);
    // Singletons
    RenderTarget *sm = ecs_field(it, RenderTarget, 3);
    RenderTarget *ot = ecs_field(it, RenderTarget, 4);

    for (int i = 0; i < it->count; i++) {
        sg_bindings bind = {
            .vertex_buffers[0] = meshes[0].vertexBuffers[0],
            .fs.images[0] = sm->depth_stencil,
            .fs.samplers[0] = ot->sampler
        };

        sg_apply_pipeline(pipelines[i].pipeline);
        sg_apply_bindings(&bind);

        sg_apply_viewport(0, 0, 512, 512, false);
        sg_draw(0, 4, 1);
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
    );

    ECS_SYSTEM(world, DrawRenderTargetPreviews, OnPassLighting,
        [in] Pipeline(up(bcview.LightingPass)),
        [in] Mesh,
        [in] RenderTarget(ShadowPass),
        [in] RenderTarget(GBufferPass),
        [none] bcview.DebugRender,
        [none] RenderPass(LightingPass)
    );

    // Pipelines, only need to create one per type
    // TODO: would be nice to setup these, especially the file paths, in script. Would require creating entities with the PipelineDescription in c, then referring to those in script. Prefabs would work well for this and meshes?
    ecs_entity_t debugShadowPipeline = ecs_set_name(world, 0, "DebugShadowPipeline");
    ecs_add_pair(world, debugShadowPipeline, EcsChildOf, ShadowPass);
    ecs_set_pair(world, debugShadowPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/shadow_default.vert"});
    ecs_set_pair(world, debugShadowPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/shadow_default.frag"});
    ecs_set(world, debugShadowPipeline, PipelineDescription, {.shader_desc = &debugShadowShaderDescription, .pipeline_desc = &debugShadowPipelineDescription});

    ecs_entity_t debugPipeline = ecs_set_name(world, 0, "DebugGBufferPipeline");
    ecs_add_pair(world, debugPipeline, EcsChildOf, GBufferPass);
    ecs_set_pair(world, debugPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/debug.vert"});
    ecs_set_pair(world, debugPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/debug.frag"});
    ecs_set(world, debugPipeline, PipelineDescription, {.shader_desc = &debugShaderDescription, .pipeline_desc = &debugPipelineDescription});

    ecs_entity_t debugRTPipeline = ecs_set_name(world, 0, "DebugRenderTargetPipeline");
    ecs_add_pair(world, debugRTPipeline, EcsChildOf, LightingPass);
    ecs_set_pair(world, debugRTPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/uv_quad.vert"});
    ecs_set_pair(world, debugRTPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/debug_RT.frag"});
    ecs_set(world, debugRTPipeline, PipelineDescription, {.shader_desc = &debugRTShaderDescription, .pipeline_desc = &debugRTPipelineDescription});

    // Render target debug rect
    // TODO: Some way to setup this in script? Could create some simple meshes in c for use in scripts
    ecs_entity_t renderTargetVis = ecs_set_name(world, 0, "RenderTargetPreview");
    ecs_add_id(world, renderTargetVis, DebugRender);
    ecs_add_pair(world, renderTargetVis, LightingPass, debugRTPipeline);

    float dbg_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    ecs_set(world, renderTargetVis, Mesh, {
        .vertexBuffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(dbg_vertices)
        })
    });

    ecs_enable(world, renderTargetVis, false);
}
