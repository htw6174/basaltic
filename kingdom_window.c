#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "ccVector.h"
#include "htw_vulkan.h"
#include "kingdom_window.h"
#include "kingdom_logicInputState.h"
#include "kingdom_worldState.h"

typedef struct WorldInfoData {
    int timeOfDay;
} WorldInfoData;

typedef struct TerrainBufferData {
    int16_t elevation;
    uint16_t palleteIndex;
    int16_t unused1; // weather / temporary effect bitmask?
    int16_t unused2; // lighting / player visibility bitmask?
} TerrainBufferData; // TODO: move this to a world logic source file; keep the data in a format that's useful for rendering (will be useful for terrain lookup and updates too)

static htw_VkContext *createWindow(unsigned int width, unsigned int height);
static void mapCamera(kd_GraphicsState *graphics, htw_PipelineHandle pipeline);

static kd_BufferPool createBufferPool(unsigned int maxCount);
static htw_Buffer* getNextBuffer(kd_BufferPool *pool);

static void createHexmapInstanceBuffers(kd_GraphicsState *graphics, kd_WorldState *world);
static void createHexmapMesh(kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain);
static void updateHexmapDescriptors(kd_GraphicsState *graphics, kd_HexmapTerrain *terrain);
static void updateHexmapDataBuffer (kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain);

static vec3 getRandomColor();

void kd_InitGraphics(kd_GraphicsState *graphics, unsigned int width, unsigned int height) {
    graphics->width = width;
    graphics->height = height;
    graphics->vkContext = createWindow(width, height);
    graphics->bufferPool = createBufferPool(100);
    graphics->frame = 0;
}

htw_VkContext *createWindow(unsigned int width, unsigned int height) {
    SDL_Window *sdlWindow = SDL_CreateWindow("kingdom", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, HTW_VK_WINDOW_FLAGS);
    htw_VkContext *context = htw_createVkContext(sdlWindow);
    return context;
}

void kd_createTerrainBuffer(kd_GraphicsState *graphics, kd_WorldState *world) {
    createHexmapMesh(graphics, world, &graphics->surfaceTerrain);
    //createHexmapInstanceBuffers(graphics, world);
    htw_finalizeBuffers(graphics->vkContext, graphics->bufferPool.count, graphics->bufferPool.buffers);
    htw_bindBuffers(graphics->vkContext, graphics->bufferPool.count, graphics->bufferPool.buffers);
    htw_updateBuffers(graphics->vkContext, graphics->bufferPool.count, graphics->bufferPool.buffers);
    updateHexmapDescriptors(graphics, &graphics->surfaceTerrain);
}

void mapCamera(kd_GraphicsState *graphics, htw_PipelineHandle pipeline) {
    htw_mapPipelinePushConstant(graphics->vkContext, pipeline, &graphics->camera);
}

int kd_renderFrame(kd_GraphicsState *graphics, kd_UiState *ui, kd_WorldState *world) {

    updateHexmapDataBuffer (graphics, world, &graphics->surfaceTerrain);
    uint32_t instanceCount = world->heightMap->width * world->heightMap->height;
    //instanceCount = min_int(instanceCount, graphics->frame % instanceCount);

    // translate camera parameters to projection and view matricies TODO: camera position interpolation, from current to target
    {
        float pitch = ui->cameraPitch * DEG_TO_RAD;
        float yaw = ui->cameraYaw * DEG_TO_RAD;
        float radius = cos(pitch) * ui->cameraDistance;
        float height = sin(pitch) * ui->cameraDistance;
        float floor = 0; //ui->activeLayer == KD_WORLD_LAYER_SURFACE ? 0 : -8;
        if (ui->cameraMode == KD_CAMERA_MODE_ORTHOGRAPHIC) {
            mat4x4Orthographic(graphics->camera.projection, 8 * ui->cameraDistance, 6 * ui->cameraDistance, 0.1f, 1000.f);
        }
        else {
            mat4x4Perspective(graphics->camera.projection, PI / 3.f, (float)graphics->width / graphics->height, 0.1f, 1000.f);
        }
        mat4x4LookAt(graphics->camera.view,
                    (vec3){ {ui->cameraX + radius * sin(yaw), ui->cameraY + radius * -cos(yaw), floor + height} },
                    (vec3){ {ui->cameraX, ui->cameraY, floor} },
                    (vec3){ {0.f, 0.f, 1.f} });
    }

    htw_Frame frame = htw_beginFrame(graphics->vkContext);

    // draw full terrain mesh
    htw_drawPipeline(graphics->vkContext, frame, graphics->surfaceTerrain.pipeline, graphics->surfaceTerrain.shaderLayout, graphics->surfaceTerrain.modelData, HTW_DRAW_TYPE_INDEXED);

    // draw instanced hexagons
    if (ui->activeLayer == KD_WORLD_LAYER_SURFACE) {
        //htw_drawPipeline(graphics->vkContext, frame, graphics->instanceTerrain.pipeline, graphics->instanceTerrain.modelData, HTW_DRAW_TYPE_INSTANCED);
    }
//     else if (ui->activeLayer == KD_WORLD_LAYER_CAVE) {
//         htw_drawPipeline(graphics->vkContext, frame, graphics->pipeline, graphics->caveInstanceBuffer, 54, instanceCount);
//     }
    htw_endFrame(graphics->vkContext, frame);

    return 0;
}

static kd_BufferPool createBufferPool(unsigned int maxCount) {
    kd_BufferPool newPool = {
        .buffers = (htw_Buffer*)malloc(sizeof(htw_Buffer) * maxCount),
        .count = 0,
        .maxCount = maxCount
    };
    return newPool;
}
static htw_Buffer* getNextBuffer(kd_BufferPool *pool) {
    if (pool->count >= pool->maxCount) return NULL;
    return &pool->buffers[pool->count++];
}

static void createHexmapInstanceBuffers(kd_GraphicsState *graphics, kd_WorldState *world) {
    int width = world->heightMap->width;
    int height = world->heightMap->height;
    uint32_t tileCount = width * height;
    uint32_t size = sizeof(vec4) * tileCount;

    kd_InstanceTerrain instanceTerrain;

    htw_ShaderInputInfo instanceInputInfo = {
        .size = sizeof(vec4),
        .offset = 0
    };
    htw_ShaderSet shaderInfo = {
        .vertexShader = htw_loadShader(graphics->vkContext, "shaders/hexagon.vert.spv"),
        .fragmentShader = htw_loadShader(graphics->vkContext, "shaders/hexagon.frag.spv"),
        .vertexInputCount = 0,
        .instanceInputStride = sizeof(vec4),
        .instanceInputCount = 1,
        .instanceInputInfos = &instanceInputInfo
    };
    htw_ShaderLayout shaderLayout = htw_createStandardShaderLayout(graphics->vkContext);
    instanceTerrain.pipeline = htw_createPipeline(graphics->vkContext, shaderLayout, shaderInfo);
    mapCamera(graphics, instanceTerrain.pipeline);

    htw_ModelData modelData = {
        .instanceBuffer = getNextBuffer(&graphics->bufferPool),
        .vertexCount = 54,
        .instanceCount = tileCount
    };
    *modelData.instanceBuffer = htw_createBuffer(graphics->vkContext, size, HTW_BUFFER_USAGE_VERTEX);
    instanceTerrain.modelData = modelData;

    graphics->instanceTerrain = instanceTerrain;

    vec4 *surfacePositions = (vec4*)modelData.instanceBuffer->hostData;
    //vec4 *cavePositions = (vec4*)modelData.instanceBuffer->hostData;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float worldX;
            float worldY;
            htw_getHexCellPositionSkewed(x, y, &worldX, &worldY);
            float height = (float)world->heightMap->values[x + (y * width)] / 10.f;
            vec4 s = {
                .x = worldX,
                .y = worldY,
                .z = height,
                .w = 0
            };
            vec4 c = {
                .x = worldX,
                .y = worldY,
                .z = -height,
                .w = 0
            };
            surfacePositions[x + (y * width)] = s;
            //cavePositions[x + (y * width)] = c;
        }
    }
}

/**
 * @brief Creates hexagonal tile surface geometry based on an input heightmap, and saves the location of each tile's geometry data for later changes
 *
 * @param terrain
 */
static void createHexmapMesh(kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain) {
    /* Overview:
     * each cell is a hexagon made of 7 verticies (one for each corner + 1 in the middle), defining 6 equilateral triangles
     * each of these triangles is divided evenly into 4 more triangles, once per subdivision (subdiv 0 = 6 tris, subdiv 1 = 24 tris)
     *
     * hexagon edges are connected to each other by a grid of quads (each quad is really 2 tris)
     * grid size is (subdivisions + 2) * (difference in tile height), in terms of heightmap increments
     * may set a minimum size or multiplier to heightmap difference for connecting edge, to add more detail on tile edges
     *
     * between 3 edge connection grids, in the corner joining 3 tiles, is a triangular area with 1-3 different edge lengths, depending on height of surrounding tiles
     * in the simplest case, this is just a single tri
     * in the most complex case, this is a tri with the longest edge = highest difference between the 3 tiles, and the sum of the lengths of the other 2 edges
     *
     * subdivisions may not be needed, use simplest version for now (hex, quad, tri)
     */

    // store reference to world heightmap
    terrain->heightMap = world->heightMap;

    // create rendering pipeline
    htw_ShaderInputInfo positionInputInfo = {
        .size = sizeof(((kd_HexmapVertexData*)0)->position),
        .offset = offsetof(kd_HexmapVertexData, position)
    };
    htw_ShaderInputInfo vertexInputInfos[] = {positionInputInfo};
    htw_ShaderSet shaderInfo = {
        .vertexShader = htw_loadShader(graphics->vkContext, "shaders/hexTerrain.vert.spv"),
        .fragmentShader = htw_loadShader(graphics->vkContext, "shaders/hexTerrain.frag.spv"),
        .vertexInputStride = sizeof(kd_HexmapVertexData),
        .vertexInputCount = 1,
        .vertexInputInfos = vertexInputInfos,
        .instanceInputCount = 0,
    };
    terrain->shaderLayout = htw_createTerrainShaderLayout(graphics->vkContext);
    terrain->pipeline = htw_createPipeline(graphics->vkContext, terrain->shaderLayout, shaderInfo);
    mapCamera(graphics, terrain->pipeline);

    // determine required size for vertex and triangle buffers
    const unsigned int width = terrain->heightMap->width;
    const unsigned int height = terrain->heightMap->height;
    const unsigned int cellCount = width * height;
    const unsigned int vertsPerCell = 7;
    const unsigned int vertexCount = vertsPerCell * cellCount;
    const unsigned int vertexDataSize = sizeof(kd_HexmapVertexData); // includes vec3 position, vec3 normal, biome information?, color?, uv?
    const unsigned int trisPerHex = 3 * 6;
    const unsigned int trisPerQuad = 3 * 2;
    const unsigned int trisPerCorner = 3 * 1;
    const unsigned int trisPerCell = 3 * ((6 * 1) + (2 * 3) + (1 * 2)); // 3 corners * 1 hexes, 3 quad edges, 2 tri corners
    const unsigned int triangleCount = trisPerCell * cellCount;
    // assign model data
    htw_ModelData modelData = {
        .vertexBuffer = getNextBuffer(&graphics->bufferPool),
        .indexBuffer = getNextBuffer(&graphics->bufferPool),
        .vertexCount = vertexCount,
        .indexCount = triangleCount
    };
    *modelData.vertexBuffer = htw_createBuffer(graphics->vkContext, vertexDataSize * vertexCount, HTW_BUFFER_USAGE_VERTEX);
    *modelData.indexBuffer = htw_createBuffer(graphics->vkContext, sizeof(uint32_t) * triangleCount, HTW_BUFFER_USAGE_INDEX);
    terrain->modelData = modelData;
    // fill mesh buffer data
    kd_HexmapVertexData *vertexData = modelData.vertexBuffer->hostData;
    uint32_t *triData = modelData.indexBuffer->hostData;
    static const float halfHeight = 0.433012701892;
    static const vec3 hexagonPositions[] = {
        { {0.0, 0.0, 0.0} }, // center
        { {0.0, 0.5, 0.0} }, // top middle, runs clockwise
        { {halfHeight, 0.25, 0.0} },
        { {halfHeight, -0.25, 0.0} },
        { {0.0, -0.5, 0.0} },
        { {-halfHeight, -0.25, 0.0} },
        { {-halfHeight, 0.25, 0.0} },
    };
    static const unsigned int hexagonIndicies[] = {
        0, 1, 2, // top right, runs clockwise
        0, 2, 3,
        0, 3, 4,
        0, 4, 5,
        0, 5, 6,
        0, 6, 1,
    };

    // create cell data texture and buffer
    terrain->worldInfoBuffer = getNextBuffer(&graphics->bufferPool);
    terrain->terrainDataBuffer = getNextBuffer(&graphics->bufferPool);
    *terrain->worldInfoBuffer = htw_createBuffer(graphics->vkContext, sizeof(WorldInfoData), HTW_BUFFER_USAGE_UNIFORM);
    *terrain->terrainDataBuffer = htw_createBuffer(graphics->vkContext, sizeof(TerrainBufferData) * cellCount, HTW_BUFFER_USAGE_STORAGE);

    // TODO: write world info
    WorldInfoData *worldInfo = terrain->worldInfoBuffer->hostData;

    // create and write model data for each cell
    TerrainBufferData *terrainData = (TerrainBufferData*)terrain->terrainDataBuffer->hostData;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned int c = x + (y * width);
            // terrain data
            TerrainBufferData cellData = {
                .elevation = getMapValue(terrain->heightMap, x, y) / 10,
                .palleteIndex = rand() % 256
            };
            terrainData[c] = cellData;
            // vertex data
            for (int v = 0; v < vertsPerCell; v++) {
                unsigned int i = (c * vertsPerCell) + v;
                float posX, posY;
                htw_getHexCellPositionSkewed(x, y, &posX, &posY);
                vec3 pos = vec3Add(hexagonPositions[v], (vec3){{posX, posY, 0.0}});
                kd_HexmapVertexData newVertex = {
                    .position = pos
                };
                vertexData[i] = newVertex;
            }
            // triangle data
            int tBase = 0; // tracks relative vert index for current part of cell mesh
            // central hexagon
            for (int t = tBase; t < trisPerHex; t++) {
                unsigned int i = (c * trisPerCell) + t;
                triData[i] = hexagonIndicies[t] + (c * vertsPerCell);
            }
            tBase += trisPerHex;
            // check if cell is on top, left, or right edge
            int rightEdge = x == width - 1;
            int topEdge = y == height - 1;
            int leftEdge = x == 0;
            // right edge quad
            if (rightEdge) {
                // fill right edge with 0s
                for (int t = tBase; t < tBase + trisPerQuad; t++) {
                    unsigned int i = (c * trisPerCell) + t;
                    triData[i] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerQuad; t+= 6) {
                    unsigned int i = (c * trisPerCell) + t;
                    // counter-clockwise from top left
                    unsigned int p1 = 2 + (c * vertsPerCell);
                    unsigned int p2 = 3 + (c * vertsPerCell);
                    unsigned int p3 = 5 + ((c + 1) * vertsPerCell);
                    unsigned int p4 = 6 + ((c + 1) * vertsPerCell);
                    triData[i + 0] = p1;
                    triData[i + 1] = p3;
                    triData[i + 2] = p2;
                    triData[i + 3] = p1;
                    triData[i + 4] = p4;
                    triData[i + 5] = p3;
                }
            }
            tBase += trisPerQuad;
            // top-right edge quad
            if (topEdge) {
                // fill edge with 0s
                for (int t = tBase; t < tBase + trisPerQuad; t++) {
                    unsigned int i = (c * trisPerCell) + t;
                    triData[i] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerQuad; t+= 6) {
                    unsigned int i = (c * trisPerCell) + t;
                    // counter-clockwise from top left
                    unsigned int p1 = 1 + (c * vertsPerCell);
                    unsigned int p2 = 2 + (c * vertsPerCell);
                    unsigned int p3 = 4 + ((c + width) * vertsPerCell);
                    unsigned int p4 = 5 + ((c + width) * vertsPerCell);
                    triData[i + 0] = p1;
                    triData[i + 1] = p3;
                    triData[i + 2] = p2;
                    triData[i + 3] = p1;
                    triData[i + 4] = p4;
                    triData[i + 5] = p3;
                }
            }
            tBase += trisPerQuad;
            // top-left edge quad
            if (leftEdge || topEdge) {
                // fill edge with 0s
                for (int t = tBase; t < tBase + trisPerQuad; t++) {
                    unsigned int i = (c * trisPerCell) + t;
                    triData[i] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerQuad; t+= 6) {
                    unsigned int i = (c * trisPerCell) + t;
                    // counter-clockwise from top left
                    unsigned int p1 = 6 + (c * vertsPerCell);
                    unsigned int p2 = 1 + (c * vertsPerCell);
                    unsigned int p3 = 3 + ((c + (width - 1)) * vertsPerCell);
                    unsigned int p4 = 4 + ((c + (width - 1)) * vertsPerCell);
                    triData[i + 0] = p1;
                    triData[i + 1] = p3;
                    triData[i + 2] = p2;
                    triData[i + 3] = p1;
                    triData[i + 4] = p4;
                    triData[i + 5] = p3;
                }
            }
            tBase += trisPerQuad;
            // top-right corner triangle
            if (rightEdge || topEdge) {
                // fill edge with 0s
                for (int t = tBase; t < tBase + trisPerCorner; t++) {
                    unsigned int i = (c * trisPerCell) + t;
                    triData[i] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerCorner; t+= 3) {
                    unsigned int i = (c * trisPerCell) + t;
                    // clockwise from current hex
                    unsigned int p1 = 2 + (c * vertsPerCell);
                    unsigned int p2 = 4 + ((c + width) * vertsPerCell);
                    unsigned int p3 = 6 + ((c + 1) * vertsPerCell);
                    triData[i + 0] = p1;
                    triData[i + 1] = p2;
                    triData[i + 2] = p3;
                }
            }
            tBase += trisPerCorner;
            // top corner triangle
            if (leftEdge || topEdge) {
                // fill edge with 0s
                for (int t = tBase; t < tBase + trisPerCorner; t++) {
                    unsigned int i = (c * trisPerCell) + t;
                    triData[i] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerCorner; t+= 3) {
                    unsigned int i = (c * trisPerCell) + t;
                    // clockwise from current hex
                    unsigned int p1 = 1 + (c * vertsPerCell);
                    unsigned int p2 = 3 + ((c + (width - 1)) * vertsPerCell);
                    unsigned int p3 = 5 + ((c + width) * vertsPerCell);
                    triData[i + 0] = p1;
                    triData[i + 1] = p2;
                    triData[i + 2] = p3;
                }
            }
        }
    }

//     // test log vert array
//     unsigned int vecsPerLine = vertsPerCell;
//     for (int i = 0; i < vertexCount; i++) {
//         vec3 v = vertexData[i].position;
//         printf("(%f, %f, %f), ", v.x, v.y, v.z);
//         // end of line
//         if (i % vecsPerLine == (vecsPerLine - 1)) printf("\n");
//     }
//     // test log tri array
//     htw_printArray(stdout, triData, sizeof(triData[0]), triangleCount, trisPerCell, "%u, ");
}

static void updateHexmapDescriptors(kd_GraphicsState *graphics, kd_HexmapTerrain *terrain){
    htw_updateTerrainDescriptors(graphics->vkContext, terrain->shaderLayout, *terrain->worldInfoBuffer, *terrain->terrainDataBuffer);
}

static void updateHexmapDataBuffer (kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain) {
    const unsigned int width = terrain->heightMap->width;
    const unsigned int height = terrain->heightMap->height;
    const unsigned int cellCount = width * height;


    TerrainBufferData *bufferData = terrain->terrainDataBuffer->hostData;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned int c = x + (y * width);
            TerrainBufferData d = {
                .elevation = getMapValue(terrain->heightMap, x, y) / 10,
                .palleteIndex = rand() % 256
            };
            bufferData[c] = d;
        }
    }

    htw_updateBuffer(graphics->vkContext, terrain->terrainDataBuffer);
}

static vec3 getRandomColor() {
    float r = (float)rand() / (float)RAND_MAX;
    float g = (float)rand() / (float)RAND_MAX;
    float b = (float)rand() / (float)RAND_MAX;
    return (vec3){ {r, g, b} };
}
