#include "basaltic_systems_view.h"
#include "basaltic_components_view.h"
#include "components/basaltic_components_planes.h"
#include "hexmapRender.h"
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
}

void SetupPipelineHexTerrain(ecs_iter_t *it) {

    char *vert = htw_load("view/shaders/hexTerrain.vert");
    char *frag = htw_load("view/shaders/hexTerrain.frag");

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

    // NOTE: when using STD140, vec4 size is required here, even though the size/alignment requirement should only be 8 bytes for a STD140 vec2 uniform. Maybe don't bother with STD140 since I only plan to use the GL backend. Can add sokol-shdc to the project later to improve cross-platform support if I really need it.
    sg_shader_uniform_block_desc fsdGlobal = {
        .size = sizeof(vec2),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "mousePosition", .type = SG_UNIFORMTYPE_FLOAT2}
        }
    };

    sg_shader_desc tsd = {
        .vs.uniform_blocks[0] = vsdGlobal,
        .vs.uniform_blocks[1] = vsdLocal,
        .vs.source = vert,
        .fs.uniform_blocks[0] = fsdGlobal,
        .fs.source = frag
    };
    sg_shader terrainShader = sg_make_shader(&tsd);

    free(vert);
    free(frag);

    sg_pipeline_desc pd = {
        .shader = terrainShader,
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT3,
                [1].format = SG_VERTEXFORMAT_USHORT2N
            }
        },
        .index_type = SG_INDEXTYPE_UINT32,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_NONE
    };
    sg_pipeline pip = sg_make_pipeline(&pd);

    u32 elementCount;
    sg_bindings bind = bc_createHexmapBindings(&elementCount);

    for (int i = 0; i < it->count; i++) {
        ecs_set(it->world, it->entities[i], Pipeline, {pip});
        ecs_set(it->world, it->entities[i], Binding, {.binding = bind, .elements = elementCount});
        ecs_set(it->world, it->entities[i], QueryDesc, {
            .desc.filter.terms = {{
                .id = ecs_id(Plane), .inout = EcsIn
            }}
        });
    }
}

void CreateModelQueries(ecs_iter_t *it) {
    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;
    QueryDesc *terms = ecs_field(it, QueryDesc, 1);

    ecs_entity_t modelPlaneId = ecs_lookup_fullpath(modelWorld, "basaltic.components.planes.Plane");
    ecs_entity_t viewPlaneId = ecs_id(Plane);
    assert(modelPlaneId == viewPlaneId);
    for (int i = 0; i < it->count; i++) {
        ecs_query_t *query = ecs_query_init(modelWorld, &(terms[i].desc));
        ecs_set(it->world, it->entities[i], ModelQuery, {query});
    }
}

void DrawPipelineHexTerrain(ecs_iter_t *it) {

    Pipeline *pips = ecs_field(it, Pipeline, 1);
    Binding *binds = ecs_field(it, Binding, 2);
    ModelQuery *queries = ecs_field(it, ModelQuery, 3);
    PVMatrix *pvs = ecs_field(it, PVMatrix, 4);
    Mouse *mouse = ecs_field(it, Mouse, 5);

    for (int i = 0; i < it->count; i++) {
        sg_apply_pipeline(pips[i].pipeline);
        sg_apply_bindings(&binds[i].binding);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));
        //vec4 mouseVec = {{mouse[i].x, mouse[i].y}};
        sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &SG_RANGE(mouse[i]));

        ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;
        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Plane *planes = ecs_field(&mit, Plane, 1);

            for (int m = 0; m < mit.count; m++) {
                // TODO: loop through chunks
                static float chunkX, chunkY;
                htw_geo_getChunkRootPosition(planes[m].chunkMap, 0, &chunkX, &chunkY);
                vec3 chunkPos = {{chunkX, chunkY, 0.0}};

                //htw_bindDescriptorSet(vkContext, hexmap->pipeline, terrain->chunkObjectDescriptorSets[c], HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_OBJECT);
                mat4x4 model;
                mat4x4SetTranslation(model, chunkPos);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 1, &SG_RANGE(model));
                sg_draw(0, binds[i].elements, 1);
            }
        }
    }
}

void BasalticSystemsViewImport(ecs_world_t *world) {
    ECS_MODULE(world, BasalticSystemsView);

    ECS_IMPORT(world, BasalticComponentsView);
    ECS_IMPORT(world, BasalticComponentsPlanes);

    // char *path = ecs_get_fullpath(world, TerrainRender);
    // printf("%s\n", path);
    // assert(ecs_lookup_fullpath(world, path) == ecs_id(TerrainRender));
    // ecs_os_free(path);

    ECS_SYSTEM(world, UniformCameraToPVMatrix, EcsPreUpdate,
        [in] Camera($),
        [in] WindowSize($),
        [out] PVMatrix($)
    );

    ECS_SYSTEM(world, SetupPipelineHexTerrain, EcsOnStart,
        [none] basaltic.components.view.TerrainRender,
        [out] !Pipeline,
        [out] !Binding,
        [out] !QueryDesc
    );

    ECS_SYSTEM(world, CreateModelQueries, EcsOnLoad,
        [in] QueryDesc,
        [in] ModelWorld($),
        [out] !ModelQuery
    );

    ECS_SYSTEM(world, DrawPipelineHexTerrain, EcsOnUpdate,
        [in] Pipeline,
        [in] Binding,
        [in] ModelQuery,
        [in] PVMatrix($),
        [in] Mouse($),
        [none] basaltic.components.view.TerrainRender,
    );

    ecs_entity_t terrainDraw = ecs_new_id(world);
    ecs_add(world, terrainDraw, TerrainRender);

    // A system with no query will run once per frame. However, an end of frame call is already being handled by the core engine. Something like this might be useful for starting and ending individual render passes
    //ECS_SYSTEM(world, SokolCommit, EcsOnStore, 0);
}
