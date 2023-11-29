#include "basaltic_systems_view.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "systems/bc_systems_view_input.h"
#include "systems/basaltic_systems_view_terrain.h"
#include "systems/basaltic_systems_view_debug.h"
#include "htw_core.h"
#include "sokol_gfx.h"
#include "basaltic_sokol_gfx.h"
#include <sys/stat.h>
#include <assert.h>

#define ECS_SYSTEM_ALIAS(world, id_, callback_, phase, ...) \
    ecs_entity_t ecs_id(id_) = 0; \
    { \
        ecs_system_desc_t desc = {0}; \
        ecs_entity_desc_t edesc = {0}; \
        edesc.id = ecs_id(id_);\
        edesc.name = #id_;\
        edesc.add[0] = ((phase) ? ecs_pair(EcsDependsOn, (phase)) : 0); \
        edesc.add[1] = (phase); \
        desc.entity = ecs_entity_init(world, &edesc);\
        desc.query.filter.expr = #__VA_ARGS__; \
        desc.callback = callback_; \
        ecs_id(id_) = ecs_system_init(world, &desc); \
    } \
    ecs_assert(ecs_id(id_) != 0, ECS_INVALID_PARAMETER, NULL); \
    ecs_entity_t id_ = ecs_id(id_);\
    (void)ecs_id(id_); \
    (void)id_;

typedef struct {
    InverseMatrices invs;
} LightingUniformsVS0;

typedef struct {
    PVMatrix cam;
    SunMatrix sun;
    vec2 invZ;
    vec3 camPos;
    vec3 toSun;
    mat4x4 iv;
    // TODO: light colors
} LightingUniformsFS0;

// TODO: should make instance buffers arbitrarially resizable
const s32 MIN_INSTANCE_COUNT = 1024;

#define COLOR_CLEAR {0.0f, 0.0f, 0.0f, 0.0f}

static sg_pass_action shadowPassAction = {
    .depth = {
        .load_action = SG_LOADACTION_CLEAR,
        .store_action = SG_STOREACTION_STORE,
        .clear_value = 1.0f
    }
};

static sg_pass_action gBufferPassAction = {
    .colors = {
        {.load_action = SG_LOADACTION_CLEAR, .clear_value = COLOR_CLEAR},
        {.load_action = SG_LOADACTION_CLEAR, .clear_value = COLOR_CLEAR},
        {.load_action = SG_LOADACTION_DONTCARE, .clear_value = COLOR_CLEAR},
    },
    .depth = {
        .load_action = SG_LOADACTION_CLEAR,
        .store_action = SG_STOREACTION_STORE,
        .clear_value = 1.0f
    }
};

static sg_pass_action lightingPassAction = {
    .colors = {
        [0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = COLOR_CLEAR},
    },
    .depth.load_action = SG_LOADACTION_DONTCARE
};

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
            {.name = "toSun", .type = SG_UNIFORMTYPE_FLOAT3},
            {.name = "inverse_view", .type = SG_UNIFORMTYPE_MAT4}
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
            .sample_type = SG_IMAGESAMPLETYPE_UNFILTERABLE_FLOAT
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
            .sampler_type = SG_SAMPLERTYPE_FILTERING
        },
        [1] = {
            .used = true,
            .sampler_type = SG_SAMPLERTYPE_NONFILTERING
        },
        [2] = {
            .used = true,
            .sampler_type = SG_SAMPLERTYPE_COMPARISON
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
            .sampler_slot = 1,
            .glsl_name = "depth"
        },
        [3] = {
            .used = true,
            .image_slot = 3,
            .sampler_slot = 2,
            .glsl_name = "shadowMap"
        }
    },
};

static sg_pipeline_desc lightingPipelineDescription = {
    .layout = {
        .attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
    },
    .depth.pixel_format = SG_PIXELFORMAT_NONE,
    .shader = {0},
    .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    .label = "lighting-pipeline",
};

static sg_pass_action transparentPassAction = {
    .colors = {
        [0] = {.load_action = SG_LOADACTION_LOAD, .clear_value = COLOR_CLEAR},
    },
    .depth.load_action = SG_LOADACTION_LOAD
};

static sg_shader_desc finalShaderDescription = {
    .fs.images = {
        [0] = {
            .used = true,
            .image_type = SG_IMAGETYPE_2D,
            .sample_type = SG_IMAGESAMPLETYPE_FLOAT
        }
    },
    .fs.samplers = {
        [0] = {
            .used = true,
            .sampler_type = SG_SAMPLERTYPE_FILTERING
        }
    },
    .fs.image_sampler_pairs = {
        [0] = {
            .used = true,
            .image_slot = 0,
            .sampler_slot = 0,
            .glsl_name = "image"
        }
    },
};

static sg_pipeline_desc finalPipelineDescription = {
    .layout = {
        .attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
    },
    .shader = {0},
    .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    .label = "final-pipeline",
};

// TODO: return sokol error code (if any)
int createRenderPass(const RenderPassDescription *rtd, RenderPass *rp, RenderTarget *rt);

int createRenderPass(const RenderPassDescription *rtd, RenderPass *rp, RenderTarget *rt) {
    *rt = (RenderTarget){0};

    sg_pass_desc pd = {0};

    for (int i = 0; i < SG_MAX_COLOR_ATTACHMENTS; i++) {
        sg_pixel_format format = rtd->colorFormats[i];
        if (format != 0) {
            // Ensure format will work as requested
            sg_pixelformat_info formatInfo = sg_query_pixelformat(format);
            assert(formatInfo.sample);
            assert(formatInfo.render);
            sg_filter filter = formatInfo.filter ? rtd->filter : SG_FILTER_NEAREST;

            rt->images[i] = sg_make_image(&(sg_image_desc){
                .render_target = true,
                .pixel_format = format,
                .width = rtd->width,
                .height = rtd->height
            });
            pd.color_attachments[i].image = rt->images[i];

            rt->samplers[i] = sg_make_sampler(&(sg_sampler_desc){
                .min_filter = filter,
                .mag_filter = filter,
                .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
                .wrap_v = SG_WRAP_CLAMP_TO_EDGE
            });
        }
    }

    if (rtd->depthFormat != 0) {
        // Ensure depth format will work as requested
        sg_pixelformat_info formatInfo = sg_query_pixelformat(rtd->depthFormat);
        assert(formatInfo.sample);
        assert(formatInfo.render);
        assert(formatInfo.depth);
        rt->depth_stencil = sg_make_image(&(sg_image_desc){
            .render_target = true,
            .pixel_format = rtd->depthFormat,
            .width = rtd->width,
            .height = rtd->height
        });
        pd.depth_stencil_attachment.image = rt->depth_stencil;

        sg_filter filter = formatInfo.filter ? rtd->filter : SG_FILTER_NEAREST;
        rt->depthSampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = filter,
            .mag_filter = filter,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .compare = rtd->compare
        });
    }

    *rp = (RenderPass){
        .action = rtd->action,
        .pass = sg_make_pass(&pd)
    };


    return 0;
}

void UniformPointerToMouse(ecs_iter_t *it) {
    Pointer *pointer = ecs_field(it, Pointer, 1);
    WindowSize *w = ecs_field(it, WindowSize, 2);

    float scale = 1.0;
    if (ecs_field_is_set(it, 3)) {
        scale = *ecs_field(it, RenderScale, 3);
    }

    float x = (pointer->x * scale) + 0.5;
    float y = ((w->y - pointer->y) * scale) + 0.5;
    ecs_singleton_set(it->world, Mouse, {x, y});
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

    // Should be minimally larger than part of terrain camera can see
    // TODO more reliable for lower cam inclinations
    float viewSize = powf(cam->distance, 2.0) + cam->origin.z;
    float width = viewSize * 8; // 4 is too low, 8 catches almost everything in view with decent shadow quality

    // Remap to tightly wrap shadow casters affecting visible area
    // min = max terrain height in world units ~= 25, at inclination = 0
    // max = width, at inclination = 90 deg
    float heightScale = sin(sun->inclination * DEG_TO_RAD);
    float height = lerp(25.0, width, heightScale);
    height = fmaxf(height, 25.0);

    mat4x4 p, v;
    mat4x4Orthographic(p, width, height, cam->zNear, cam->zFar);

    // Don't follow camera z, and position in middle of terrain z range, so frustrum can always cover terrain z extents
    vec3 camCenter = {.xy = cam->origin.xy, .z = 12.5};
    mat4x4LookAt(v,
        vec3Add(sunPosition, camCenter),
        camCenter,
        (vec3){{0.f, 0.f, 1.f}}
    );

    mat4x4MultiplyMatrix(*pv, v, p);
    ecs_singleton_modified(it->world, SunMatrix);
}

void InitRenderPasses(ecs_iter_t *it) {
    RenderPassDescription *rpds = ecs_field(it, RenderPassDescription, 1);

    for (int i = 0; i < it->count; i++) {
        RenderPass rp;
        RenderTarget rt;
        createRenderPass(&rpds[i], &rp, &rt);
        ecs_set_ptr(it->world, it->entities[i], RenderPass, &rp);
        ecs_set_ptr(it->world, it->entities[i], RenderTarget, &rt);
    }
}

void OnRenderSizeChanged(ecs_iter_t *it) {
    WindowSize *ws = ecs_field(it, WindowSize, 1);
    RenderScale *rs = ecs_field(it, RenderScale, 2);

    // clamp render scale between 0.05 and 2
    float scale = fminf(fmaxf(0.05, *rs), 2.0);
    s32 width = ws->x * scale;
    s32 height = ws->y * scale;

    // Update render pass descriptions for offscreen passes
    RenderPassDescription *gBufDesc = ecs_get_mut(it->world, GBufferPass, RenderPassDescription);
    RenderPassDescription *lightingDesc = ecs_get_mut(it->world, LightingPass, RenderPassDescription);
    gBufDesc->width = width;
    gBufDesc->height = height;
    lightingDesc->width = width;
    lightingDesc->height = height;
    ecs_modified(it->world, GBufferPass, RenderPassDescription);
    ecs_modified(it->world, LightingPass, RenderPassDescription);
}

void RefreshRenderPasses(ecs_iter_t *it) {
    RenderPassDescription *rpds = ecs_field(it, RenderPassDescription, 1);

    for (int i = 0; i < it->count; i++) {

        const RenderTarget *rt = ecs_get(it->world, it->entities[i], RenderTarget);
        if (rt != NULL) {
            // destroy color attachments
            for (int q = 0; q < SG_MAX_COLOR_ATTACHMENTS; q++) {
                if (rt->images[q].id != 0) {
                    sg_destroy_image(rt->images[q]);
                }
                if (rt->samplers[q].id != 0) {
                    sg_destroy_sampler(rt->samplers[q]);
                }
            }
            // destroy depth attachment if exists
            if (rt->depth_stencil.id != 0) {
                sg_destroy_image(rt->depth_stencil);
            }
            if (rt->depth_stencil.id != 0) {
                sg_destroy_sampler(rt->depthSampler);
            }
        }

        const RenderPass *rp = ecs_get(it->world, it->entities[i], RenderPass);
        if (rp != NULL) {
            sg_destroy_pass(rp->pass);
        }

        RenderPass new_rp;
        RenderTarget new_rt;
        createRenderPass(&rpds[i], &new_rp, &new_rt);
        ecs_set_ptr(it->world, it->entities[i], RenderPass, &new_rp);
        ecs_set_ptr(it->world, it->entities[i], RenderTarget, &new_rt);
    }
}

// TEST: single-use system to make a transparent pass with the lighting pass's color attachment and gbuffer's depth attachment
void RefreshTransparentPass(ecs_iter_t *it) {
    // This only triggers when the lighting render target is set, on the lighting pass entity, but requires the transparent pass entity to have component set. For this reason, ignore fields and just get constant source components here
    const RenderTarget *gBufferTarget = ecs_get(it->world, GBufferPass, RenderTarget);
    const RenderTarget *lightingTarget = ecs_get(it->world, LightingPass, RenderTarget);
    RenderPass *transparentPass = ecs_get_mut(it->world, TransparentPass, RenderPass);

    if (transparentPass != NULL) {
        sg_destroy_pass(transparentPass->pass);
    }

    // Color from lighting, depth from gbuffer
    *transparentPass = (RenderPass){
        .pass = sg_make_pass(&(sg_pass_desc){
            .color_attachments[0].image = lightingTarget->images[0],
            .depth_stencil_attachment.image = gBufferTarget->depth_stencil
        }),
        .action = transparentPassAction
    };
    ecs_modified(it->world, TransparentPass, RenderPass);
}

void BeginRenderPass(ecs_iter_t *it) {
    RenderPass *rp = ecs_field(it, RenderPass, 1);

    //printf("Begin Pass: %s\n", ecs_get_name(it->world, it->system));
    sg_begin_pass(rp[0].pass, &rp[0].action);
}

void EndRenderPass(ecs_iter_t *it) {
    sg_end_pass();
}

void BeginFinalPass(ecs_iter_t *it) {
    WindowSize *ws = ecs_field(it, WindowSize, 1);

    sg_pass_action pa = {0};

    sg_begin_default_pass(&pa, ws->x, ws->y);
}

void EndFinalPass(ecs_iter_t *it) {
    sg_end_pass();
}

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
        } else {
            printf("Failed creating pipeline for entity %s. Error: %i", ecs_get_name(it->world, it->entities[i]), err);
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
        ecs_query_desc_t desc = {.filter.expr = terms[i].expr};
        ecs_query_t *query = ecs_query_init(modelWorld, &desc);
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
        ecs_query_desc_t desc = {
            .parent = baseQueries[i].query,
            .filter.expr = terms[i].expr
        };
        ecs_query_t *query = ecs_query_init(modelWorld, &desc);
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

void InitInstanceBuffers(ecs_iter_t *it) {
    InstanceBuffer *ibs = ecs_field(it, InstanceBuffer, 1);
    ecs_entity_t instanceBufferTarget = ecs_pair_second(it->world, ecs_field_id(it, 1));
    const EcsComponent *instanceComponent = ecs_get(it->world, instanceBufferTarget, EcsComponent);
    if (instanceComponent == NULL) {
        ecs_err("Cannot create buffer for InstanceBuffer target %s which is not a component", ecs_get_name(it->world, instanceBufferTarget));
        return;
    }

    for (int i = 0; i < it->count; i++) {
        s32 maxInstances = MAX(MIN_INSTANCE_COUNT, ibs[i].maxInstances);
        size_t instanceDataSize = instanceComponent->size * maxInstances;

        // TODO: Make component destructor to free memory
        void *instanceData = malloc(instanceDataSize);

        sg_buffer_desc vbd = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
            .size = instanceDataSize
        };
        sg_buffer instanceBuffer = sg_make_buffer(&vbd);

        ibs[i] = (InstanceBuffer){
            .maxInstances = maxInstances,
            .instances = 0,
            .buffer = instanceBuffer,
            .size = instanceDataSize,
            .data = instanceData
        };
    }
}

void DrawInstanceShadows(ecs_iter_t *it) {
    Pipeline *pipelines = ecs_field(it, Pipeline, 1);
    InstanceBuffer *ibs = ecs_field(it, InstanceBuffer, 2);
    Elements *eles = ecs_field(it, Elements, 3);
    SunMatrix *pvs = ecs_field(it, SunMatrix, 4);
    WrapInstanceOffsets *wraps = ecs_field(it, WrapInstanceOffsets, 5);

    for (int i = 0; i < it->count; i++) {
        if (ibs[i].data == NULL) {
            continue;
        }
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
    // constant sources
    PVMatrix *pvs = ecs_field(it, PVMatrix, 4);
    WrapInstanceOffsets *wraps = ecs_field(it, WrapInstanceOffsets, 5);
    Clock *programClock = ecs_field(it, Clock, 6);

    for (int i = 0; i < it->count; i++) {
        if (ibs[i].data == NULL) {
            continue;
        }
        sg_bindings bind = {
            .vertex_buffers[0] = ibs[i].buffer
        };

        sg_apply_pipeline(pipelines[i].pipeline);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));
        sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &SG_RANGE(*programClock));

        bc_drawWrapInstances(0, eles[i].count, ibs[i].instances, 1, (vec3){{0.0, 0.0, 0.0}}, wraps[i].offsets);
    }
}

void DrawLighting(ecs_iter_t *it) {
    Pipeline *pipelines = ecs_field(it, Pipeline, 1);
    Mesh *meshes = ecs_field(it, Mesh, 2);
    RenderTarget *gBufferTarget = ecs_field(it, RenderTarget, 3);
    RenderTarget *shadowTarget = ecs_field(it, RenderTarget, 4);
    // Singletons
    PVMatrix *pv = ecs_field(it, PVMatrix, 5);
    InverseMatrices *invs = ecs_field(it, InverseMatrices, 6);
    SunMatrix *sun = ecs_field(it, SunMatrix, 7);
    Camera *cam = ecs_field(it, Camera, 8);
    SunLight *light = ecs_field(it, SunLight, 9);

    for (int i = 0; i < it->count; i++) {
        sg_bindings bind = {
            .vertex_buffers[0] = meshes[0].vertexBuffers[0],
            .fs.images = {
                gBufferTarget[0].images[0],
                gBufferTarget[0].images[1],
                gBufferTarget[0].depth_stencil,
                shadowTarget[0].depth_stencil
            },
            .fs.samplers = {
                gBufferTarget[0].samplers[0],
                gBufferTarget[0].depthSampler,
                shadowTarget[0].depthSampler
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
            // TODO: Should store and retrieve these positions instead of trying to recalc them
            .camPos = vec3Add(bc_sphereToCartesian(cam->yaw, cam->pitch, powf(cam->distance, 2.0)), cam->origin),
            .toSun = bc_sphereToCartesian(light->azimuth, light->inclination, 1.0)
        };
        mat4x4Copy(lu0.cam, pv[0]);
        mat4x4Copy(lu0.sun, sun[0]);
        mat4x4Copy(lu0.iv, invs[0].view);

        sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &SG_RANGE(lu0));

        sg_draw(0, 4, 1);
    }
}

void DrawFinal(ecs_iter_t *it) {
    Pipeline *pipelines = ecs_field(it, Pipeline, 1);
    Mesh *meshes = ecs_field(it, Mesh, 2);
    RenderTarget *rt = ecs_field(it, RenderTarget, 3);

    sg_bindings bind = {
        .vertex_buffers[0] = meshes->vertexBuffers[0],
        .fs.images = {
            rt->images[0]
        },
        .fs.samplers = {
            rt->samplers[0]
        }
    };

    sg_apply_pipeline(pipelines->pipeline);
    sg_apply_bindings(&bind);

    sg_draw(0, 4, 1);
}

void CycleRingBuffers(ecs_iter_t *it) {
    RingBuffer *rbs = ecs_field(it, RingBuffer, 1);

    for (int i = 0; i < it->count; i++) {
        // current readableBuffer becomes next writableBuffer
        rbs[i].writableBuffer = rbs[i].readableBuffer;
        // next readableBuffer is the oldest writableBuffer
        rbs[i].readableBuffer = rbs[i]._buffers[rbs[i]._head];
        rbs[i]._head  = (rbs[i]._head + 1) % RING_BUFFER_LENGTH;
    }
}

// Backup to periodically update view with model data if changes didn't cuase renderOutdated to be set
void ExtraModelRefresh(ecs_iter_t *it) {
    ModelWorld *mw = ecs_field(it, ModelWorld, 1);

    mw->renderOutdated = true;
}

void BcviewSystemsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewSystems);

    ECS_IMPORT(world, Bcview);
    ECS_IMPORT(world, BcviewPhases);
    ECS_IMPORT(world, BcviewSystemsInput);
    ECS_IMPORT(world, BcviewSystemsDebug);
    ECS_IMPORT(world, BcviewSystemsTerrain);

    ECS_SYSTEM(world, UniformPointerToMouse, EcsPreUpdate,
        [in] Pointer($),
        [in] WindowSize($),
        [in] ?RenderScale(VideoSettings),
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

    // Render pass setup and updates
    // TODO: need systems to setup the renderpassdescriptions for specific passes, and assign sizes based on window size, video settings, etc.
    // requires these inputs:
    // [in] WindowSize($),
    // [in] RenderScale(VideoSettings),
    // [in] ShadowMapSize(VideoSettings),

    // NOTE: not really needed as observer can handle setup
    // ECS_SYSTEM(world, InitRenderPasses, EcsOnStart,
    //     [in] RenderPassDescription,
    //     [out] !RenderPass,
    //     [out] !RenderTarget
    // );

    // Render pass reinitialization observers
    // Render targets need to be resized if the screen size changes, and corresponding pass needs to be recreated
    // FIXME: need to be careful with observers, some situations where new field value isn't available when observer is triggered
    // FIXME: observer doesn't trigger from setting RenderScale?
    // TODO observer to change render pass description sizes with screen size, graphics settings, etc.
    ECS_OBSERVER(world, OnRenderSizeChanged, EcsOnSet,
        [in] WindowSize($),
        [in] RenderScale(VideoSettings),
    );

    ECS_OBSERVER(world, RefreshRenderPasses, EcsOnSet,
        [in] RenderPassDescription
    );

    ECS_OBSERVER(world, RefreshTransparentPass, EcsOnSet,
        [in] RenderTarget(LightingPass)
    );

    // FIXME: disabling observers does nothing?
    //ecs_enable(world, RefreshRenderPasses, false);
    //ecs_add_id(world, RefreshRenderPasses, EcsDisabled);

    // Render pass begin and end
    // Begin and end are the same for all but the default (final) pass, but need to run in different phases. Here multiple systems are created with the same callback but different phases and data sources to handle this
    // Give both begin and end the same requirements, simply so they can both be disabled easily
    // Example with manual system creation:
    // ecs_entity_t ecs_id(BeginShadowPass) = ecs_system(world, {
    //     .entity = ecs_entity(world, {
    //         .name = "BeginShadowPass",
    //         .add = {ecs_dependson(PreShadowPass)}
    //     }),
    //     .query.filter.expr = "[in] (RenderPass, ShadowPass)(RenderPasses)",
    //     .callback = BeginRenderPass
    // });

    ECS_SYSTEM_ALIAS(world, BeginShadowPass, BeginRenderPass, BeginPassShadow,
        [in] RenderPass(ShadowPass)
    );

    ECS_SYSTEM_ALIAS(world, EndShadowPass, EndRenderPass, EndPassShadow,
        [none] RenderPass(ShadowPass)
    );

    ECS_SYSTEM_ALIAS(world, BeginGBufferPass, BeginRenderPass, BeginPassGBuffer,
        [in] RenderPass(GBufferPass)
    );

    ECS_SYSTEM_ALIAS(world, EndGBufferPass, EndRenderPass, EndPassGBuffer,
        [none] RenderPass(GBufferPass)
    );

    ECS_SYSTEM_ALIAS(world, BeginLightingPass, BeginRenderPass, BeginPassLighting,
        [in] RenderPass(LightingPass)
    );

    ECS_SYSTEM(world, DrawLighting, OnPassLighting,
        [in] Pipeline(up(bcview.LightingPass)),
        [in] Mesh,
        [in] RenderTarget(GBufferPass),
        [in] RenderTarget(ShadowPass),
        [in] PVMatrix($),
        [in] InverseMatrices($),
        [in] SunMatrix($),
        [in] Camera($),
        [in] SunLight($),
        [none] bcview.InternalRender,
        [none] RenderPass(LightingPass)
    );

    ECS_SYSTEM_ALIAS(world, EndLightingPass, EndRenderPass, EndPassLighting,
        [none] RenderPass(LightingPass)
    );

    ECS_SYSTEM_ALIAS(world, BeginTransparentPass, BeginRenderPass, BeginPassTransparent,
        [in] RenderPass(TransparentPass)
    );

    ECS_SYSTEM_ALIAS(world, EndTransparentPass, EndRenderPass, EndPassTransparent,
        [none] RenderPass(TransparentPass)
    );


    // For final pass, don't need 3 different phases because systems run in definition order. However, no new draw systems can (or should) be added to this pass
    ECS_SYSTEM(world, BeginFinalPass, OnPassFinal,
        [in] WindowSize($)
    );

    ECS_SYSTEM(world, DrawFinal, OnPassFinal,
        [in] Pipeline(up(bcview.FinalPass)),
        [in] Mesh,
        [in] RenderTarget(LightingPass),
        [none] bcview.InternalRender
    );

    ECS_SYSTEM(world, EndFinalPass, OnPassFinal,
        [none] WindowSize($)
    );

    ECS_SYSTEM(world, SetupPipelines, EcsOnUpdate,
        [inout] (ResourceFile, bcview.VertexShaderSource),
        [inout] (ResourceFile, bcview.FragmentShaderSource),
        [in] PipelineDescription,
        [out] !Pipeline,
    );

    ECS_SYSTEM(world, ReloadPipelines, EcsOnUpdate,
        [inout] (ResourceFile, bcview.VertexShaderSource),
        [inout] (ResourceFile, bcview.FragmentShaderSource),
        [in] PipelineDescription,
        [out] Pipeline,
    );

    ECS_SYSTEM(world, ExtraModelRefresh, EcsPreUpdate,
        [out] ModelWorld($)
    );

    // Limit refresh and reload to once per second
    ecs_entity_t reloadTick = ecs_set_interval(world, 0, 1.0);
    ecs_set_tick_source(world, ReloadPipelines, reloadTick);
    ecs_set_tick_source(world, ExtraModelRefresh, reloadTick);

    // TODO: Should enable/disable by default based on build type
    //ecs_enable(world, ReloadPipelines, false);
    ecs_enable(world, ExtraModelRefresh, false);

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

    // NOTE: the * wildcard works here, while the _ (any) wildcard will never match without another term present (why?)
    ECS_OBSERVER(world, InitInstanceBuffers, EcsOnAdd,
        [inout] (InstanceBuffer, *)
    );

    // TODO: could easily make this an alias of the base DrawInstances callback, just need to remove the SunMatrix as a separate component, instead make component of a sun singleton
    ECS_SYSTEM(world, DrawInstanceShadows, OnPassShadow,
        [in] Pipeline(up(bcview.ShadowPass)),
        [in] (InstanceBuffer, _),
        [in] Elements,
        [in] SunMatrix($),
        [in] WrapInstanceOffsets($),
        [none] ShadowPass($)
    );

    // TODO: experiment with GroupBy to cluster draws with the same pipeline and/or bindings
    ECS_SYSTEM_ALIAS(world, DrawInstancesGBuffer, DrawInstances, OnPassGBuffer,
        [in] Pipeline(up(bcview.GBufferPass)), // NOTE: the source specifier "up(GBufferPass)" gets component from the entity which is target of GBufferPass relationship for $this
        [in] (InstanceBuffer, _),
        [in] Elements,
        [in] PVMatrix($),
        [in] WrapInstanceOffsets($),
        [in] Clock($)
    );

    ECS_SYSTEM_ALIAS(world, DrawInstancesTransparent, DrawInstances, OnPassTransparent,
        [in] Pipeline(up(bcview.TransparentPass)),
        [in] (InstanceBuffer, _),
        [in] Elements,
        [in] PVMatrix($),
        [in] WrapInstanceOffsets($),
        [in] Clock($)
    );

    // TODO: on load or on store?
    ECS_SYSTEM(world, CycleRingBuffers, EcsOnStore,
        [inout] RingBuffer
    );

    //ecs_enable(world, DrawInstances, false);

    // A system with no query will run once per frame. However, an end of frame call is already being handled by the core engine. Something like this might be useful for starting and ending individual render passes
    //ECS_SYSTEM(world, SokolCommit, EcsOnStore, 0);

    // Pass setup
    ecs_set(world, ShadowPass, RenderPassDescription, {
        .action = shadowPassAction,
        .width = 2048,
        .height = 2048,
        .depthFormat = SG_PIXELFORMAT_DEPTH,
        .filter = SG_FILTER_NEAREST,
        .compare = SG_COMPAREFUNC_LESS
    });

    ecs_set(world, GBufferPass, RenderPassDescription, {
        .action = gBufferPassAction,
        .width = 800,
        .height = 600,
        .colorFormats = {
            SG_PIXELFORMAT_RGBA8,
            SG_PIXELFORMAT_RGBA16F,
            SG_PIXELFORMAT_RG16SI
        },
        .depthFormat = SG_PIXELFORMAT_DEPTH,
        .filter = SG_FILTER_LINEAR
    });

    ecs_set(world, LightingPass, RenderPassDescription, {
        .action = lightingPassAction,
        .width = 800,
        .height = 600,
        .colorFormats = {
            SG_PIXELFORMAT_RGBA8
        },
        .filter = SG_FILTER_NEAREST
    });

    // ecs_set(world, TransparentPass, RenderPassDescription, {
    //     .action = transparentPassAction,
    //     .width = 800,
    //     .height = 600,
    //     .colorFormats = {
    //         SG_PIXELFORMAT_RGBA8
    //     },
    //     .depthFormat = SG_PIXELFORMAT_DEPTH,
    //     .filter = SG_FILTER_LINEAR
    // });

    // Lighting and final pass pipeline setup
    // UV quad for drawing from a render target to another render target or the screen
    float quadVerts[] = { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f };
    sg_buffer quadBuffer = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quadVerts)
    });

    ecs_entity_t lightingPipeline = ecs_set_name(world, 0, "LightingPipeline");
    ecs_add_pair(world, lightingPipeline, EcsChildOf, LightingPass);
    ecs_set_pair(world, lightingPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/lighting.vert"});
    ecs_set_pair(world, lightingPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/lighting.frag"});
    ecs_set(world, lightingPipeline, PipelineDescription, {.pipeline_desc = &lightingPipelineDescription, .shader_desc = &lightingShaderDescription});

    ecs_entity_t lightingQuad = ecs_set_name(world, 0, "LightingQuad");
    ecs_add_id(world, lightingQuad, InternalRender);
    ecs_add_pair(world, lightingQuad, LightingPass, lightingPipeline);
    ecs_set(world, lightingQuad, Mesh, {
        .vertexBuffers[0] = quadBuffer
    });

    ecs_entity_t finalPipeline = ecs_set_name(world, 0, "FinalPipeline");
    ecs_add_pair(world, finalPipeline, EcsChildOf, FinalPass);
    ecs_set_pair(world, finalPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/uv_quad.vert"});
    ecs_set_pair(world, finalPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/final.frag"});
    ecs_set(world, finalPipeline, PipelineDescription, {.pipeline_desc = &finalPipelineDescription, .shader_desc = &finalShaderDescription});

    ecs_entity_t finalQuad = ecs_set_name(world, 0, "FinalQuad");
    ecs_add_pair(world, finalQuad, FinalPass, finalPipeline);
    ecs_add_id(world, finalQuad, InternalRender);
    ecs_set(world, finalQuad, Mesh, {
        .vertexBuffers[0] = quadBuffer
    });
}
