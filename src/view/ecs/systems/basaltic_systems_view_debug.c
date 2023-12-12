#include "basaltic_systems_view_debug.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "basaltic_components.h"
#include "basaltic_worldGen.h"
#include "ccVector.h"
#include "sokol_gfx.h"
#include "basaltic_sokol_gfx.h"

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
    // Global uniforms
    .fs.uniform_blocks[0] = {
        .size = sizeof(float),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "time", .type = SG_UNIFORMTYPE_FLOAT}
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
    .color_count = 1,
    .colors = {
        [0] = {
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
            }
        }
    }
};

static sg_pipeline_desc debugArrowPipelineDescription = {
    .shader = {0},
    .layout = {
        .buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
        .attrs = {
            [0].format = SG_VERTEXFORMAT_FLOAT3,
            [1].format = SG_VERTEXFORMAT_FLOAT3,
            [2].format = SG_VERTEXFORMAT_FLOAT4,
            [3].format = SG_VERTEXFORMAT_FLOAT,
            [4].format = SG_VERTEXFORMAT_FLOAT,
        }
    },
    .depth = {
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true
    },
    .cull_mode = SG_CULLMODE_NONE,
    .color_count = 1,
    .colors = {
        [0] = {
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
            }
        }
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
        .sampler_type = SG_SAMPLERTYPE_COMPARISON
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

void UpdateDebugBuffers(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);
    InstanceBuffer *instanceBuffers = ecs_field(it, InstanceBuffer, 2);
    // Optional term, may be null
    Color *colors = ecs_field(it, Color, 3);
    // constant source
    const vec3 *scale = ecs_field(it, Scale, 4);
    ecs_world_t *modelWorld = ecs_field(it, ModelWorld, 5)->world;

    for (int i = 0; i < it->count; i++) {
        DebugInstanceData *instanceData = instanceBuffers[i].data;
        if (instanceData == NULL) {
            continue;
        }
        u32 instanceCount = 0;

        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Position *positions = ecs_field(&mit, Position, 1);
            htw_ChunkMap *cm = ecs_field(&mit, Plane, 2)->chunkMap;
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

void UpdateArrowBuffers(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);
    InstanceBuffer *instanceBuffers = ecs_field(it, InstanceBuffer, 2);
    // Optional term, may be null
    Color *colors = ecs_field(it, Color, 3);
    // constant source
    const vec3 *scale = ecs_field(it, Scale, 4);
    ecs_world_t *modelWorld = ecs_field(it, ModelWorld, 5)->world;

    for (int i = 0; i < it->count; i++) {
        ArrowInstanceData *instanceData = instanceBuffers[i].data;
        if (instanceData == NULL) {
            continue;
        }
        u32 instanceCount = 0;

        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Position *positions = ecs_field(&mit, Position, 1);
            Position *destinations = ecs_field(&mit, Destination, 2);
            // constant source
            Plane *plane = ecs_field(&mit, Plane, 3);
            htw_ChunkMap *cm = plane->chunkMap;
            for (int m = 0; m < mit.count && instanceCount < instanceBuffers[i].maxInstances; m++) {
                CellData *posCell = htw_geo_getCell(cm, positions[m]);
                // NOTE: behavior systems that pre-wrap destination coords create arrows stretching across the entire world when crossing a wrap boundary
                CellData *destCell = htw_geo_getCell(cm, destinations[m]);
                float startX, startY, endX, endY;
                htw_geo_getHexCellPositionSkewed(positions[m], &startX, &startY);
                htw_geo_getHexCellPositionSkewed(destinations[m], &endX, &endY);
                instanceData[instanceCount] = (ArrowInstanceData){
                    .start = vec3MultiplyVector((vec3){{startX, startY, posCell->height + 1}}, *scale),
                    .end = vec3MultiplyVector((vec3){{endX, endY, destCell->height + 1}}, *scale),
                    .color = colors == NULL ? (vec4){{1.0, 0.0, 1.0, 1.0}} : colors[i],
                    .width = 0.2,
                    .speed = 1.0
                };
                instanceCount++;
            }
        }
        instanceBuffers[i].instances = instanceCount;
        sg_update_buffer(instanceBuffers[i].buffer, &(sg_range){.ptr = instanceData, .size = instanceBuffers[i].size});
    }
}

void UpdateRiverArrowBuffers(ecs_iter_t *it) {
    InstanceBuffer *instanceBuffers = ecs_field(it, InstanceBuffer, 1);
    // Optional term, may be null
    Color *colors = ecs_field(it, Color, 2);
    // constant source
    const vec3 *scale = ecs_field(it, Scale, 3);
    ecs_world_t *modelWorld = ecs_field(it, ModelWorld, 4)->world;
    const FocusPlane *fp = ecs_field(it, FocusPlane, 5);

    for (int i = 0; i < it->count; i++) {
        ArrowInstanceData *instanceData = instanceBuffers[i].data;
        if (instanceData == NULL) {
            continue;
        }
        u32 instanceCount = 0;

        const Plane *plane = ecs_get(modelWorld, fp->entity, Plane);
        htw_ChunkMap *cm = plane->chunkMap;

        const float lineWidth = 0.03;

        for (int y = 0; y < cm->mapHeight; y++) {
            for (int x = 0; x < cm->mapWidth && instanceCount < instanceBuffers[i].maxInstances - 1; x++) { // may create 2 instances so need to stop just short of max
                htw_geo_GridCoord cellCoord = {x, y};
                CellData *cell = htw_geo_getCell(cm, cellCoord);
                // quickly eliminate cells with no nearby rivers
                if (*(u32*)(&cell->waterways) == 0) {
                    continue;
                }
                s32 hSelf = cell->height;
                CellWaterConnections connections = bc_extractCellWaterways(cm, cellCoord);
                float posX, posY;
                htw_geo_getHexCellPositionSkewed(cellCoord, &posX, &posY);
                // world space position; z should be at least 0, don't need to draw river down into ocean
                vec3 cellPos = {{posX, posY, MAX(0, cell->height + 1)}};
                for (int d = 0; d < HEX_DIRECTION_COUNT; d++) {
                    // world space origin of lines on this edge
                    s32 c1 = d;
                    vec3 relStart = {{htw_geo_hexCornerPositions[c1][0], htw_geo_hexCornerPositions[c1][1], 0.0}};
                    const float contract = 0.5; // bring rivers toward center of cell
                    vec3 start = vec3Add(cellPos, vec3Multiply(relStart, contract));

                    if (connections.segments[d]) {
                        s32 c2 = (c1 + 1) % HEX_CORNER_COUNT;
                        vec3 relEnd = {{htw_geo_hexCornerPositions[c2][0], htw_geo_hexCornerPositions[c2][1], 0.0}};
                        vec3 end = vec3Add(cellPos, vec3Multiply(relEnd, contract));

                        // Get slope from all 3 other cells touching this edge; factor with existing connections to determine flow speed of segment
                        s32 dLeft = htw_geo_hexDirectionLeft(d);
                        s32 dRight = htw_geo_hexDirectionRight(d);
                        // heights
                        s32 hTwin = connections.neighbors[d]->height;
                        s32 hLeft = connections.neighbors[dLeft]->height;
                        s32 hRight = connections.neighbors[dRight]->height;
                        // slopes
                        s32 sTwin = hTwin - hSelf;
                        s32 sLeft = hLeft - hSelf;
                        s32 sRight = hRight - hSelf;

                        // positive slope towards left in and twin out: + flow speed
                        // positive slope towards right out and twin in: - flow speed
                        // if both twin connections: contribution cancels out
                        s32 speed = 0;
                        speed += sLeft  * (connections.connectionsIn[dLeft] > 0);
                        speed += sTwin  * (connections.connectionsOut[d] > 0);
                        speed -= sTwin  * (connections.connectionsIn[d] > 0);
                        speed -= sRight * (connections.connectionsOut[dRight] > 0);

                        // As wide as widest connection
                        // TODO: segment width should be additive from inflow connections only and propogate towards outflows; alternatively, could make constant based on inflow for entire cell
                        float widthMult = MAX(
                            MAX(connections.connectionsIn[dLeft], connections.connectionsOut[d]),
                            MAX(connections.connectionsIn[d], connections.connectionsOut[dRight])
                        );
                        widthMult = MAX(widthMult, 1.0);

                        instanceData[instanceCount] = (ArrowInstanceData){
                            .start = vec3MultiplyVector(start, *scale),
                            .end = vec3MultiplyVector(end, *scale),
                            .color = colors == NULL ? (vec4){{1.0, 0.0, 1.0, 1.0}} : colors[i],
                            .width = lineWidth * widthMult,
                            .speed = speed
                        };
                        instanceCount++;
                    }
                    if (connections.connectionsOut[d] > 0) {
                        // get neighboring cell's worldspace position
                        htw_geo_GridCoord neighborCoord = POSITION_IN_DIRECTION(cellCoord, d);
                        float posX2, posY2;
                        htw_geo_getHexCellPositionSkewed(neighborCoord, &posX2, &posY2);
                        s32 hTwin = connections.neighbors[d]->height;
                        vec3 neighborPos = {{posX2, posY2, MAX(0, hTwin + 1)}};

                        // Opposite corner from end of segment
                        s32 cTwin = (d + 4) % HEX_CORNER_COUNT;

                        vec3 relEnd = {{htw_geo_hexCornerPositions[cTwin][0], htw_geo_hexCornerPositions[cTwin][1], 0.0}};
                        vec3 end = vec3Add(neighborPos, vec3Multiply(relEnd, contract));

                        s32 sTwin = hTwin - hSelf;

                        instanceData[instanceCount] = (ArrowInstanceData){
                            .start = vec3MultiplyVector(start, *scale),
                            .end = vec3MultiplyVector(end, *scale),
                            .color = colors == NULL ? (vec4){{1.0, 0.0, 1.0, 1.0}} : colors[i],
                            .width = lineWidth * connections.connectionsOut[d],
                            .speed = -sTwin
                        };
                        instanceCount++;
                    }
                }
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
            .fs.samplers[0] = ot->samplers[0]
        };

        sg_apply_pipeline(pipelines[i].pipeline);
        sg_apply_bindings(&bind);

        //sg_apply_viewport(0, 0, 512, 512, false);
        sg_draw(0, 4, 1);
    }
}

void BcviewSystemsDebugImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewSystemsDebug);

    ECS_IMPORT(world, Bcview);
    ECS_IMPORT(world, BcviewPhases);

    ECS_SYSTEM(world, UpdateDebugBuffers, OnModelChanged,
        [in] ModelQuery,
        [inout] (InstanceBuffer, DebugInstanceData),
        [in] ?Color,
        [in] Scale($),
        [in] ModelWorld($),
        [none] bcview.DebugRender
    );

    ECS_SYSTEM(world, UpdateArrowBuffers, OnModelChanged,
        [in] ModelQuery,
        [inout] (InstanceBuffer, ArrowInstanceData),
        [in] ?Color,
        [in] Scale($),
        [in] ModelWorld($),
        [none] bcview.DebugRender
    );

    ECS_SYSTEM(world, UpdateRiverArrowBuffers, OnModelChanged,
        [inout] (InstanceBuffer, ArrowInstanceData),
        [in] ?Color,
        [in] Scale($),
        [in] ModelWorld($),
        [in] FocusPlane($),
        [none] bcview.DebugRender,
        [none] bcview.TerrainRender
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

    ecs_entity_t debugPipeline = ecs_set_name(world, 0, "DebugTransparentPipeline");
    ecs_add_pair(world, debugPipeline, EcsChildOf, TransparentPass);
    ecs_set_pair(world, debugPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/debug.vert"});
    ecs_set_pair(world, debugPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/debug.frag"});
    ecs_set(world, debugPipeline, PipelineDescription, {.shader_desc = &debugShaderDescription, .pipeline_desc = &debugPipelineDescription});

    ecs_entity_t debugArrowPipeline = ecs_set_name(world, 0, "DebugArrowPipeline");
    ecs_add_pair(world, debugArrowPipeline, EcsChildOf, TransparentPass);
    ecs_set_pair(world, debugArrowPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/debugArrow.vert"});
    ecs_set_pair(world, debugArrowPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/debugArrow.frag"});
    ecs_set(world, debugArrowPipeline, PipelineDescription, {.shader_desc = &debugShaderDescription, .pipeline_desc = &debugArrowPipelineDescription});

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
