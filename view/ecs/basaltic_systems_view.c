#include "basaltic_systems_view.h"
#include "basaltic_components_view.h"
#include "components/basaltic_components_planes.h"
#include "hexmapRender.h"
#include "htw_core.h"
#include "sokol_gfx.h"

void updateTerrainVisibleChunks(htw_ChunkMap *chunkMap, TerrainBuffer *terrain, u32 centerChunk);
void updateHexmapDataBuffer(htw_ChunkMap *chunkMap, TerrainBuffer *terrain, u32 chunkIndex, u32 subBufferIndex);

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

void bc_drawWrapInstances(int base_element, int num_elements, int num_instances, int modelMatrixUniformBlockIndex, vec3 position, const WrapInstanceOffsets *offsets) {
    static mat4x4 model;
    for (int i = 0; i < 4; i++) {
        mat4x4SetTranslation(model, vec3Add(position, offsets->offsets[i]));
        sg_apply_uniforms(SG_SHADERSTAGE_VS, modelMatrixUniformBlockIndex, &SG_RANGE(model));
        sg_draw(base_element, num_elements, num_instances);
    }
}

void updateTerrainVisibleChunks(htw_ChunkMap *chunkMap, TerrainBuffer *terrain, u32 centerChunk) {
    // update list a to contain all elements of list b while changing as few elements as possible of list a
    // a = [0, 1], b = [1, 2] : put all elements of b into a with fewest location changes (a = [2, 1])
    // b doesn't contain 0, so 0 must be replaced
    // b contains 1, so 1 must stay in place
    // b contains 2, so 2 must be inserted into a
    // complexity shaping up to be n^2 at least, any way to do better?
    // for every element of a: need to know if it definitely isn't in b
    // for every element of b: need to know it is already in a
    // update a and b with this information: a = [-1, 1], b = [-1, 2]
    // loop through a and b at the same time: advance through a until a negative value is found, then advance through b until a positive value is found, and move it to a. Repeat from last position through all of a
    // closestChunks.chunkIndex[] = b
    // loadedChunks.chunkIndex[] = a
    // NOTE: there *has* to be a better way to do this, feels very messy right now.

    s32 *closestChunks = terrain->closestChunks;
    s32 *loadedChunks = terrain->loadedChunks;

    s32 minChunkOffset = 1 - (s32)terrain->renderedChunkRadius;
    s32 maxChunkOffset = (s32)terrain->renderedChunkRadius;
    for (int c = 0, y = minChunkOffset; y < maxChunkOffset; y++) {
        for (int x = minChunkOffset; x < maxChunkOffset; x++, c++) {
            closestChunks[c] = htw_geo_getChunkIndexAtOffset(chunkMap, centerChunk, (htw_geo_GridCoord){x, y});
        }
    }

    // compare closest chunks to currently loaded chunks
    for (int i = 0; i < terrain->renderedChunkCount; i++) {
        s32 loadedTarget = loadedChunks[i];
        u8 foundMatch = 0;
        for (int k = 0; k < terrain->renderedChunkCount; k++) {
            u32 requiredTarget = closestChunks[k];
            if (requiredTarget != -1 && loadedTarget == requiredTarget) {
                // found a position in a that already contains an element of b
                // mark b[k] -1 so we know it doesn't need to be touched later
                closestChunks[k] = -1;
                foundMatch = 1;
                break;
            }
        }
        if (!foundMatch) {
            // a[i] needs to be replaced with an element of b
            loadedChunks[i] = -1;
        }
    }

    // move all positive values from b into negative values of a
    u32 ia = 0, ib = 0;
    while (ia < terrain->renderedChunkCount) {
        if (loadedChunks[ia] == -1) {
            // find next positive value in b
            while (1) {
                if (closestChunks[ib] != -1) {
                    loadedChunks[ia] = closestChunks[ib];
                    break;
                }
                ib++;
            }
        }
        ia++;
    }

    for (int c = 0; c < terrain->renderedChunkCount; c++) {
        // TODO: only update chunk if freshly visible or has pending updates
        updateHexmapDataBuffer(chunkMap, terrain, loadedChunks[c], c);
    }
}

void updateHexmapDataBuffer(htw_ChunkMap *chunkMap, TerrainBuffer *terrain, u32 chunkIndex, u32 subBufferIndex) {
    const u32 width = bc_chunkSize;
    const u32 height = bc_chunkSize;
    htw_Chunk *chunk = &chunkMap->chunks[chunkIndex];

    bc_CellData *cellData = terrain->data + (terrain->chunkBufferCellCount * subBufferIndex);

    // get primary chunk data
    for (s32 y = 0; y < height; y++) {
        for (s32 x = 0; x < width; x++) {
            u32 c = x + (y * width);
            bc_CellData *cell = bc_getCellByIndex(chunkMap, chunkIndex, c);
            cellData[c] = *cell;
        }
    }

    u32 edgeDataIndex = width * height;
    // get chunk data from chunk at x + 1
    u32 rightChunk = htw_geo_getChunkIndexAtOffset(chunkMap, chunkIndex, (htw_geo_GridCoord){1, 0});
    chunk = &chunkMap->chunks[rightChunk];
    for (s32 y = 0; y < height; y++) {
        u32 c = edgeDataIndex + y; // right edge of this row
        bc_CellData *cell = bc_getCellByIndex(chunkMap, rightChunk, y * chunkMap->chunkSize);
        cellData[c] = *cell;
    }
    edgeDataIndex += height;

    // get chunk data from chunk at y + 1
    u32 topChunk = htw_geo_getChunkIndexAtOffset(chunkMap, chunkIndex, (htw_geo_GridCoord){0, 1});
    chunk = &chunkMap->chunks[topChunk];
    for (s32 x = 0; x < width; x++) {
        u32 c = edgeDataIndex + x;
        bc_CellData *cell = bc_getCellByIndex(chunkMap, topChunk, x);
        cellData[c] = *cell;
    }
    edgeDataIndex += width;

    // get chunk data from chunk at x+1, y+1 (only one cell)
    u32 topRightChunk = htw_geo_getChunkIndexAtOffset(chunkMap, chunkIndex, (htw_geo_GridCoord){1, 1});
    chunk = &chunkMap->chunks[topRightChunk];
    bc_CellData *cell = bc_getCellByIndex(chunkMap, topRightChunk, 0);
    cellData[edgeDataIndex] = *cell;

    size_t chunkDataSize = terrain->chunkBufferCellCount *  sizeof(bc_CellData);
    size_t chunkDataOffset = chunkDataSize * subBufferIndex;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain->gluint);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, chunkDataOffset, chunkDataSize, cellData);
}

void SetupPipelineHexTerrain(ecs_iter_t *it) {
    RenderDistance *rd = ecs_field(it, RenderDistance, 1);

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

        u32 visibilityRadius = rd[i].radius;
        u32 visibilityDiameter = (visibilityRadius * 2) - 1;
        u32 renderedChunkCount = visibilityDiameter * visibilityDiameter;

        u32 chunkBufferCellCount = (bc_chunkSize * bc_chunkSize) + (2 * bc_chunkSize) + 1;
        size_t bufferSize = chunkBufferCellCount * renderedChunkCount * sizeof(bc_CellData);

        GLuint terrainBuffer;
        glGenBuffers(1, &terrainBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrainBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, NULL, GL_DYNAMIC_DRAW);

        ecs_set(it->world, it->entities[i], TerrainBuffer, {
            .gluint = terrainBuffer,
            .renderedChunkRadius = visibilityRadius,
            .renderedChunkCount = renderedChunkCount,
            .closestChunks = calloc(renderedChunkCount, sizeof(u32)),
            .loadedChunks = calloc(renderedChunkCount, sizeof(u32)),
            .chunkBufferCellCount = chunkBufferCellCount,
            .data = malloc(bufferSize)
        });
    }

    sg_reset_state_cache();
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

void DrawPipelineHexTerrain(ecs_iter_t *it) {
    Pipeline *pips = ecs_field(it, Pipeline, 1);
    Binding *binds = ecs_field(it, Binding, 2);
    ModelQuery *queries = ecs_field(it, ModelQuery, 3);
    TerrainBuffer *terrainBuffers = ecs_field(it, TerrainBuffer, 4);
    PVMatrix *pvs = ecs_field(it, PVMatrix, 5);
    Mouse *mouse = ecs_field(it, Mouse, 6);
    FeedbackBuffer *feedback = ecs_field(it, FeedbackBuffer, 7);
    HoveredCell *hovered = ecs_field(it, HoveredCell, 8);

    const WrapInstanceOffsets *wraps = ecs_singleton_get(it->world, WrapInstanceOffsets);

    for (int i = 0; i < it->count; i++) {
        sg_apply_pipeline(pips[i].pipeline);
        sg_apply_bindings(&binds[i].binding);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));
        sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &SG_RANGE(mouse[i]));

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, feedback[i].gluint);

        ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;
        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Plane *planes = ecs_field(&mit, Plane, 1);

            for (int m = 0; m < mit.count; m++) {
                // TODO: correctly determine center chunk from camera focus
                u32 centerChunk = bc_getChunkIndexByWorldPosition(planes[i].chunkMap, 0.0f, 0.0f);
                updateTerrainVisibleChunks(planes[i].chunkMap, &terrainBuffers[i], centerChunk);

                htw_ChunkMap *cm = planes[i].chunkMap;
                for (int c = 0; c < cm->chunkCountX * cm->chunkCountY; c++) {
                    u32 chunkIndex = terrainBuffers[i].loadedChunks[c];
                    static float chunkX, chunkY;
                    htw_geo_getChunkRootPosition(cm, chunkIndex, &chunkX, &chunkY);
                    vec3 chunkPos = {{chunkX, chunkY, 0.0}};

                    size_t range = terrainBuffers[i].chunkBufferCellCount * sizeof(bc_CellData);
                    size_t offset = c * range;
                    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, terrainBuffers[i].gluint, offset, range);

                    //if (glGetError()) printf("%x\n", glGetError());

                    if (wraps != NULL) {
                        bc_drawWrapInstances(0, binds[i].elements, 1, 1, chunkPos, wraps);
                    } else {
                        mat4x4 model;
                        mat4x4SetTranslation(model, chunkPos);
                        sg_apply_uniforms(SG_SHADERSTAGE_VS, 1, &SG_RANGE(model));
                        sg_draw(0, binds[i].elements, 1);
                    }
                }
            }
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, feedback[i].gluint);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(hovered[i]), &hovered[i]);
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
        [in] RenderDistance($),
        [out] !Pipeline,
        [out] !Binding,
        [out] !QueryDesc,
        [out] !TerrainBuffer,
        [none] basaltic.components.view.TerrainRender,
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

    ECS_SYSTEM(world, DrawPipelineHexTerrain, EcsOnUpdate,
        [in] Pipeline,
        [in] Binding,
        [in] ModelQuery,
        [in] TerrainBuffer,
        [in] PVMatrix($),
        [in] Mouse($),
        [in] FeedbackBuffer($),
        [out] HoveredCell($),
        [none] ModelWorld($),
        [none] basaltic.components.view.TerrainRender,
    );

    GLuint feedbackBuffer;
    glGenBuffers(1, &feedbackBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, feedbackBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(HoveredCell), NULL, GL_DYNAMIC_READ);

    sg_reset_state_cache();

    ecs_singleton_set(world, FeedbackBuffer, {feedbackBuffer});

    ecs_entity_t terrainDraw = ecs_new_id(world);
    ecs_add(world, terrainDraw, TerrainRender);

    // TODO: create this buffer, then add component to terrainDraw after model is attached

    // A system with no query will run once per frame. However, an end of frame call is already being handled by the core engine. Something like this might be useful for starting and ending individual render passes
    //ECS_SYSTEM(world, SokolCommit, EcsOnStore, 0);
}
