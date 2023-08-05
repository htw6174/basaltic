#include "basaltic_systems_view.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "systems/basaltic_systems_view_terrain.h"
#include "systems/basaltic_systems_view_debug.h"
#include "htw_core.h"
#include "sokol_gfx.h"
#include "basaltic_sokol_gfx.h"
#include <sys/stat.h>

typedef struct {
    InverseMatrices invs;
} LightingUniformsVS0;

typedef struct {
    PVMatrix cam;
    SunMatrix sun;
    vec2 invZ;
    vec3 camPos;
    vec3 toSun;
    // TODO: light colors
} LightingUniformsFS0;

static sg_shader_desc lightingShaderDescription = {
    .vs.uniform_blocks[0] = {
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .size = sizeof(LightingUniformsVS0),
        .uniforms = {
            {.name = "inverse_view", .type = SG_UNIFORMTYPE_MAT4},
            {.name = "inverse_projection", .type = SG_UNIFORMTYPE_MAT4},
        }
    },
    .fs.uniform_blocks[0] = {
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .size = sizeof(LightingUniformsFS0),
        .uniforms = {
            {.name = "pv", .type = SG_UNIFORMTYPE_MAT4},
            {.name = "sun", .type = SG_UNIFORMTYPE_MAT4},
            {.name = "invZ", .type = SG_UNIFORMTYPE_FLOAT2},
            {.name = "camera_position", .type = SG_UNIFORMTYPE_FLOAT3},
            {.name = "toSun", .type = SG_UNIFORMTYPE_FLOAT3}
        }
    },
    .fs.images = {
        [0] = {
            .used = true,
            .image_type = SG_IMAGETYPE_2D,
            .sample_type = SG_IMAGESAMPLETYPE_FLOAT
        },
        [1] = {
            .used = true,
            .image_type = SG_IMAGETYPE_2D,
            .sample_type = SG_IMAGESAMPLETYPE_FLOAT
        },
        [2] = {
            .used = true,
            .image_type = SG_IMAGETYPE_2D,
            .sample_type = SG_IMAGESAMPLETYPE_DEPTH
        },
        [3] = {
            .used = true,
            .image_type = SG_IMAGETYPE_2D,
            .sample_type = SG_IMAGESAMPLETYPE_DEPTH
        }
    },
    .fs.samplers = {
        [0] = {
            .used = true,
            .sampler_type = SG_SAMPLERTYPE_SAMPLE
        },
        [1] = {
            .used = true,
            .sampler_type = SG_SAMPLERTYPE_COMPARE
        }
    },
    .fs.image_sampler_pairs = {
        [0] = {
            .used = true,
            .image_slot = 0,
            .sampler_slot = 0,
            .glsl_name = "diffuse"
        },
        [1] = {
            .used = true,
            .image_slot = 1,
            .sampler_slot = 0,
            .glsl_name = "normal"
        },
        [2] = {
            .used = true,
            .image_slot = 2,
            .sampler_slot = 0,
            .glsl_name = "depth"
        },
        [3] = {
            .used = true,
            .image_slot = 3,
            .sampler_slot = 1,
            .glsl_name = "shadowMap"
        }
    },
};

static sg_pipeline_desc lightingPipelineDescription = {
    .layout = {
        .attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
    },
    .shader = {0},
    .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    .label = "lighting-pipeline",
};

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
    InverseMatrices *invs = ecs_singleton_get_mut(it->world, InverseMatrices);

    // TODO: figure out if inner loops should be used for singleton-only systems
    //for (int i = 0; i < it->count; i++) {
    {
        vec3 cameraPosition = bc_sphereToCartesian(cam->yaw, cam->pitch, powf(cam->distance, 2.0));
        cameraPosition = vec3Add(cameraPosition, cam->origin);

        mat4x4 p, v;
        mat4x4Perspective(p, PI / 3.f, (float)w->x / w->y, cam->zNear, cam->zFar);
        mat4x4LookAt(v,
            cameraPosition,
            cam->origin,
            (vec3){ {0.f, 0.f, 1.f} }
        );

        mat4x4MultiplyMatrix(pv[0], v, p);

        // inverse matricies
        mat4x4Inverse(invs->view, v);
        mat4x4Inverse(invs->projection, p);
    }

    ecs_singleton_modified(it->world, PVMatrix);
    ecs_singleton_modified(it->world, InverseMatrices);
}

void UniformSunToMatrix(ecs_iter_t *it) {
    const SunLight *sun = ecs_singleton_get(it->world, SunLight);
    const Camera *cam = ecs_singleton_get(it->world, Camera);
    SunMatrix *pv = ecs_singleton_get_mut(it->world, SunMatrix);

    vec3 sunPosition = bc_sphereToCartesian(sun->azimuth, sun->inclination, cam->zFar / 2.0);

    mat4x4 p, v;
    mat4x4Orthographic(p, sun->projectionSize, sun->projectionSize, cam->zNear, cam->zFar);
    mat4x4LookAt(v,
        vec3Add(sunPosition, cam->origin),
        cam->origin,
        (vec3){{0.f, 0.f, 1.f}}
    );

    mat4x4MultiplyMatrix(*pv, v, p);
    ecs_singleton_modified(it->world, SunMatrix);
}

void SetupRenderPass(ecs_iter_t *it) {
    WindowSize *ws = ecs_field(it, WindowSize, 1);

    int width = ws->x;
    int height = ws->y;

    OffscreenTargets ots = {
        .diffuse = sg_make_image(&(sg_image_desc){
            .render_target = true,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .width = width,
            .height = height,
            .sample_count = 1
        }),
        .normal = sg_make_image(&(sg_image_desc){
            .render_target = true,
            .pixel_format = SG_PIXELFORMAT_RGBA16F,
            .width = width,
            .height = height,
            .sample_count = 1
        }),
        .depth = sg_make_image(&(sg_image_desc){
            .render_target = true,
            .pixel_format = SG_PIXELFORMAT_R32F,
            .width = width,
            .height = height,
            .sample_count = 1
        }),
        .zbuffer = sg_make_image(&(sg_image_desc){
            .render_target = true,
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .width = width,
            .height = height,
            .sample_count = 1
        }),
        .sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        })
    };

    // Used instead of singleton_set for convenience of setting with direct pointer
    ecs_set_ptr(it->world, ecs_id(OffscreenTargets), OffscreenTargets, &ots);

    sg_color clear = {0.0f, 0.0f, 0.0f, 0.0f};
    ecs_singleton_set(it->world, RenderPass, {
        .action = {
            .colors = {
                {.load_action = SG_LOADACTION_CLEAR, .clear_value = clear},
                {.load_action = SG_LOADACTION_CLEAR, .clear_value = clear},
                {.load_action = SG_LOADACTION_CLEAR, .clear_value = clear},
            },
            .depth = {
                .load_action = SG_LOADACTION_CLEAR,
                .store_action = SG_STOREACTION_STORE,
                .clear_value = 1.0f
            }
        },
        .pass = sg_make_pass(&(sg_pass_desc){
            .color_attachments = {
                [0].image = ots.diffuse,
                [1].image = ots.normal,
                [2].image = ots.depth,
            },
            .depth_stencil_attachment.image = ots.zbuffer,
            .label = "render-pass"
        })
    });
}

void BeginShadowPass(ecs_iter_t *it) {
    ShadowPass *sp = ecs_field(it, ShadowPass, 1);

    sg_begin_pass(sp[0].pass, &sp[0].action);
};

void EndShadowPass(ecs_iter_t *it) {
    sg_end_pass();
};

void BeginRenderPass(ecs_iter_t *it) {
    RenderPass *rp = ecs_field(it, RenderPass, 1);

    sg_begin_pass(rp[0].pass, &rp[0].action);
};

void EndRenderPass(ecs_iter_t *it) {
    sg_end_pass();
};

void BeginFinalPass(ecs_iter_t *it) {
    WindowSize *ws = ecs_field(it, WindowSize, 1);

    sg_pass_action pa = {0};

    sg_begin_default_pass(&pa, ws->x, ws->y);
};

void EndFinalPass(ecs_iter_t *it) {
    sg_end_pass();
};

void SetupPipelines(ecs_iter_t *it) {
    ResourceFile *vertSources = ecs_field(it, ResourceFile, 1);
    ResourceFile *fragSources = ecs_field(it, ResourceFile, 2);
    PipelineDescription *pipelineDescriptions = ecs_field(it, PipelineDescription, 3);

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

void DrawInstanceShadows(ecs_iter_t *it) {
    Pipeline *pipelines = ecs_field(it, Pipeline, 1);
    InstanceBuffer *ibs = ecs_field(it, InstanceBuffer, 2);
    Elements *eles = ecs_field(it, Elements, 3);
    SunMatrix *pvs = ecs_field(it, SunMatrix, 4);
    WrapInstanceOffsets *wraps = ecs_field(it, WrapInstanceOffsets, 5);

    for (int i = 0; i < it->count; i++) {
        sg_bindings bind = {
            .vertex_buffers[0] = ibs[0].buffer
        };

        sg_apply_pipeline(pipelines[i].pipeline);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));

        bc_drawWrapInstances(0, eles[i].count, ibs[i].instances, 1, (vec3){{0.0, 0.0, 0.0}}, wraps[i].offsets);
    }
}

void DrawInstances(ecs_iter_t *it) {
    Pipeline *pipelines = ecs_field(it, Pipeline, 1);
    InstanceBuffer *ibs = ecs_field(it, InstanceBuffer, 2);
    Elements *eles = ecs_field(it, Elements, 3);
    PVMatrix *pvs = ecs_field(it, PVMatrix, 4);
    WrapInstanceOffsets *wraps = ecs_field(it, WrapInstanceOffsets, 5);

    for (int i = 0; i < it->count; i++) {
        sg_bindings bind = {
            .vertex_buffers[0] = ibs[0].buffer
        };

        sg_apply_pipeline(pipelines[i].pipeline);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));

        bc_drawWrapInstances(0, eles[i].count, ibs[i].instances, 1, (vec3){{0.0, 0.0, 0.0}}, wraps[i].offsets);
    }
}

void DrawLighting(ecs_iter_t *it) {
    Pipeline *pipelines = ecs_field(it, Pipeline, 1);
    Mesh *meshes = ecs_field(it, Mesh, 2);
    // Singletons
    OffscreenTargets *ots = ecs_field(it, OffscreenTargets, 3);
    ShadowMap *sm = ecs_field(it, ShadowMap, 4);
    PVMatrix *pv = ecs_field(it, PVMatrix, 5);
    InverseMatrices *invs = ecs_field(it, InverseMatrices, 6);
    SunMatrix *sun = ecs_field(it, SunMatrix, 7);
    Camera *cam = ecs_field(it, Camera, 8);
    SunLight *light = ecs_field(it, SunLight, 9);

    for (int i = 0; i < it->count; i++) {
        sg_bindings bind = {
            .vertex_buffers[0] = meshes[0].vertexBuffers[0],
            .fs.images = {
                ots[0].diffuse,
                ots[0].normal,
                ots[0].zbuffer,
                //sm[0].image,
                sm[0].image
            },
            .fs.samplers = {
                ots[0].sampler,
                sm[0].sampler
            }
        };

        sg_apply_pipeline(pipelines[i].pipeline);
        sg_apply_bindings(&bind);

        LightingUniformsVS0 lvs0 = {0};
        // /mat4x4Inverse(lvs0.invs.view, invs[0].view);
        mat4x4Copy(lvs0.invs.view, invs[0].view);
        mat4x4Copy(lvs0.invs.projection, invs[0].projection);

        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(lvs0));

        LightingUniformsFS0 lu0 = {
            .invZ = {
                .x = -(cam->zFar + cam->zNear) / (cam->zFar - cam->zNear),
                .y = -(2.f * cam->zFar * cam->zNear) / (cam->zFar - cam->zNear)
            },
            .camPos = bc_sphereToCartesian(cam->yaw, cam->pitch, powf(cam->distance, 2.0)),
            .toSun = bc_sphereToCartesian(light->azimuth, light->inclination, 1.0)
        };
        mat4x4Copy(lu0.cam, pv[0]);
        mat4x4Copy(lu0.cam, sun[0]);

        sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &SG_RANGE(lu0));

        sg_draw(0, 4, 1);
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
        [out] ?Mouse($)
    );

    ECS_SYSTEM(world, UniformCameraToPVMatrix, EcsPreUpdate,
        [in] Camera($),
        [in] WindowSize($),
        [out] ?PVMatrix($),
        [out] ?InverseMatrices($)
    );

    ECS_SYSTEM(world, UniformSunToMatrix, EcsPreUpdate,
        [in] SunLight($),
        [in] Camera($),
        [out] ?SunMatrix($)
    );

    // Render pass setup
    // Some setup depends on screen initialization, and render targets need to be resized if the screen size changes
    ECS_SYSTEM(world, SetupRenderPass, EcsPreUpdate,
        [in] WindowSize($),
        [out] !OffscreenTargets($),
        [out] !RenderPass($)
    );

    // Render pass begin and end
    // Give both begin and end the same requirements, simply so they can both be disabled easily
    ECS_SYSTEM(world, BeginShadowPass, PreShadowPass,
        [in] ShadowPass($)
    );

    ECS_SYSTEM(world, EndShadowPass, PostShadowPass,
        [none] ShadowPass($)
    );

    ECS_SYSTEM(world, BeginRenderPass, PreRenderPass,
        [in] RenderPass($)
    );

    ECS_SYSTEM(world, EndRenderPass, PostRenderPass,
        [none] RenderPass($)
    );

    ECS_SYSTEM(world, BeginFinalPass, OnLightingPass,
        [in] WindowSize($)
    );

    ECS_SYSTEM(world, DrawLighting, OnLightingPass,
        [in] Pipeline,
        [in] Mesh,
        [in] OffscreenTargets($),
        [in] ShadowMap($),
        [in] PVMatrix($),
        [in] InverseMatrices($),
        [in] SunMatrix($),
        [in] Camera($),
        [in] SunLight($),
        [none] bcview.LightingPipeline
    );

    ECS_SYSTEM(world, EndFinalPass, OnLightingPass,
        [none] WindowSize($)
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

    ECS_SYSTEM(world, DrawInstanceShadows, OnShadowPass,
        [in] Pipeline(up(bcview.ShadowPipeline)),
        [in] InstanceBuffer,
        [in] Elements,
        [in] SunMatrix($),
        [in] WrapInstanceOffsets($),
        [none] ShadowPass($),
    );

    // TODO: experiment with GroupBy to cluster draws with the same pipeline and/or bindings
    ECS_SYSTEM(world, DrawInstances, OnRenderPass,
        [in] Pipeline(up(bcview.RenderPipeline)), // NOTE: the source specifier "up(RenderPipeline)" gets component from target of RenderPipeline relationship for $this
        [in] InstanceBuffer,
        [in] Elements,
        [in] PVMatrix($),
        [in] WrapInstanceOffsets($),
    );

    //ecs_enable(world, DrawInstances, false);

    // A system with no query will run once per frame. However, an end of frame call is already being handled by the core engine. Something like this might be useful for starting and ending individual render passes
    //ECS_SYSTEM(world, SokolCommit, EcsOnStore, 0);

    // Shadow pass setup
    sg_image shadowMap = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 2048,
        .height = 2048,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 1,
        .label = "shadow-map",
    });

    ecs_singleton_set(world, ShadowMap, {
        .image = shadowMap,
        .sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .compare = SG_COMPAREFUNC_LESS,
            .label = "shadow-sampler",
        })
    });

    ecs_singleton_set(world, ShadowPass, {
        .action = {
            .depth = {
                .load_action = SG_LOADACTION_CLEAR,
                .store_action = SG_STOREACTION_STORE,
                .clear_value = 1.0f
            }
        },
        .pass = sg_make_pass(&(sg_pass_desc){
            .depth_stencil_attachment.image = shadowMap,
            .label = "shadow-pass"
        })
    });

    // Render pass setup

    // Combined pipeline and mesh to draw lit world on a screen uv quad
    ecs_entity_t finalPass = ecs_set_name(world, 0, "Final Pass");
    ecs_add(world, finalPass, LightingPipeline);
    ecs_set_pair(world, finalPass, ResourceFile, VertexShaderSource,   {.path = "view/shaders/lighting.vert"});
    ecs_set_pair(world, finalPass, ResourceFile, FragmentShaderSource, {.path = "view/shaders/lighting.frag"});
    ecs_set(world, finalPass, PipelineDescription, {.pipeline_desc = &lightingPipelineDescription, .shader_desc = &lightingShaderDescription});

    float dbg_vertices[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    ecs_set(world, finalPass, Mesh, {
        .vertexBuffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(dbg_vertices)
        })
    });
}
