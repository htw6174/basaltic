
#include "hexmapRender.h"
#include "htw_core.h"
#include "htw_vulkan.h"
#include "htw_geomap.h"
#include "basaltic_mesh.h"
#include "basaltic_logic.h"
#include "basaltic_worldState.h"

static void createHexmapMesh(htw_VkContext *vkContext, bc_RenderableHexmap *hexmap, htw_BufferPool bufferPool, htw_DescriptorSetLayout perFrameLayout, htw_DescriptorSetLayout perPassLayout);
static void updateHexmapDataBuffer(htw_VkContext *vkContext, htw_ChunkMap *chunkMap, bc_HexmapTerrain *terrain, u32 chunkIndex, u32 subBufferIndex);
static void setHexmapBufferFromCell(bc_CellData *cell, bc_TerrainCellData *target);

bc_RenderableHexmap *bc_createRenderableHexmap(htw_VkContext *vkContext, htw_BufferPool bufferPool, htw_DescriptorSetLayout perFrameLayout, htw_DescriptorSetLayout perPassLayout) {
    bc_RenderableHexmap *newHexmap = calloc(1, sizeof(bc_RenderableHexmap));
    createHexmapMesh(vkContext, newHexmap, bufferPool, perFrameLayout, perPassLayout);
    //createHexmapInstanceBuffers(rc, world);
    return newHexmap;
}

bc_HexmapTerrain *bc_createHexmapTerrain(htw_VkContext *vkContext, htw_BufferPool bufferPool, bc_RenderableHexmap *hexmap, u32 visibilityRadius) {
    bc_HexmapTerrain *newTerrain = calloc(1, sizeof(bc_HexmapTerrain));
    newTerrain->renderedChunkRadius = visibilityRadius;
    u32 visibilityDiameter = (visibilityRadius * 2) - 1;
    u32 renderedChunkCount = visibilityDiameter * visibilityDiameter;
    newTerrain->renderedChunkCount = renderedChunkCount;
    newTerrain->closestChunks = calloc(renderedChunkCount, sizeof(u32));
    newTerrain->loadedChunks = calloc(renderedChunkCount, sizeof(u32));
    for (int i = 0; i < renderedChunkCount; i++) {
        newTerrain->loadedChunks[i] = -1;
    }

    // create descriptor set for each chunk, to swap between during each frame
    newTerrain->chunkObjectDescriptorSets = malloc(sizeof(htw_DescriptorSet) * renderedChunkCount);
    htw_allocateDescriptors(vkContext, hexmap->chunkObjectLayout, renderedChunkCount, newTerrain->chunkObjectDescriptorSets);

    // best way to handle using multiple sub-buffers and binding a different one each draw call?
    // Options:
    // a. create a descriptor set for every sub buffer, and set a different buffer offset for each. When drawing, most descriptors sets can be bound once per frame. A new terrain data descriptor set will be bound before every draw call.
    // b. create one descriptor set with a dynamic storage buffer. When drawing, bind the terrain data descriptor set with a different dynamic offset before every draw call.
    // because I don't need to change the size of these sub buffers at runtime, there shouldn't be much of a difference between these approaches.
    // include strips of adjacent chunk data in buffer size
    u32 bufferCellCount = (bc_chunkSize * bc_chunkSize) + (2 * bc_chunkSize) + 1;
    // Chunk data includes the chunk index out front
    newTerrain->terrainBufferSize = offsetof(bc_TerrainBufferData, chunkData) + (sizeof(bc_CellData) * bufferCellCount);

    newTerrain->terrainBufferData = malloc(newTerrain->terrainBufferSize * renderedChunkCount);
    newTerrain->terrainBuffer = htw_createSplitBuffer(vkContext, bufferPool, newTerrain->terrainBufferSize, renderedChunkCount, HTW_BUFFER_USAGE_STORAGE);

    return newTerrain;
}

/**
 * @brief Creates featureless hexagonal tile surface geometry, detail is added later by shaders
 *
 * @param terrain
 */
static void createHexmapMesh(htw_VkContext *vkContext, bc_RenderableHexmap *hexmap, htw_BufferPool bufferPool, htw_DescriptorSetLayout perFrameLayout, htw_DescriptorSetLayout perPassLayout) {
    /* Overview:
     * each cell is a hexagon made of 7 verticies (one for each corner + 1 in the middle), defining 6 equilateral triangles
     * each of these triangles is divided evenly into 4 more triangles, once per subdivision (subdiv 0 = 6 tris, subdiv 1 = 24 tris)
     *
     * hexagon edges are connected to each other by a grid of quads
     *
     * between 3 edge connection grids, in the corner joining 3 tiles, is a triangular area
     * (also use subdivisions for edges and corners?)
     *
     * The mesh has one extra tri strip on the top and right sides, so that the gaps between chunks can be filled seamlessly
     */

    // create rendering pipeline
    htw_ShaderInputInfo positionInputInfo = {
        .size = sizeof(((bc_HexmapVertexData*)0)->position), // pointer casting trick to get size of a struct member
        .offset = offsetof(bc_HexmapVertexData, position),
        .inputType = HTW_VERTEX_TYPE_FLOAT
    };
    htw_ShaderInputInfo cellInputInfo = {
        .size = sizeof(((bc_HexmapVertexData*)0)->cellIndex),
        .offset = offsetof(bc_HexmapVertexData, cellIndex),
        .inputType = HTW_VERTEX_TYPE_UINT
    };
    htw_ShaderInputInfo vertexInputInfos[] = {positionInputInfo, cellInputInfo};
    htw_ShaderSet shaderInfo = {
        .vertexShader = htw_loadShader(vkContext, "shaders_bin/hexTerrain.vert.spv"),
        .fragmentShader = htw_loadShader(vkContext, "shaders_bin/hexTerrain.frag.spv"),
        .vertexInputStride = sizeof(bc_HexmapVertexData),
        .vertexInputCount = 2,
        .vertexInputInfos = vertexInputInfos,
        .instanceInputCount = 0,
    };

    hexmap->chunkObjectLayout = htw_createTerrainObjectSetLayout(vkContext);
    htw_DescriptorSetLayout terrainPipelineLayouts[] = {perFrameLayout, perPassLayout, NULL, hexmap->chunkObjectLayout};
    hexmap->pipeline = htw_createPipeline(vkContext, terrainPipelineLayouts, shaderInfo);

    // determine required size for vertex and triangle buffers
    const u32 width = bc_chunkSize;
    const u32 height = bc_chunkSize;
    const u32 cellCount = width * height;
    const u32 vertsPerCell = 7;
    // for connecting to adjacent chunks
    const u32 rightSideVerts = height * 3;
    const u32 topSideVerts = width * 3;
    const u32 topRightCornerVerts = 1;
    const u32 vertexCount = (vertsPerCell * cellCount) + rightSideVerts + topSideVerts + topRightCornerVerts;
    const u32 vertexSize = sizeof(bc_HexmapVertexData);
    // NOTE: tri variables contain the number of indicies for each area, not number of triangles
    const u32 trisPerHex = 3 * 6;
    const u32 trisPerQuad = 3 * 2;
    const u32 trisPerCorner = 3 * 1;
    const u32 trisPerCell = 3 * ((6 * 1) + (2 * 3) + (1 * 2)); // 3 corners * 1 hexes, 3 quad edges, 2 tri corners
    // for connecting to adjacent chunks TODO: don't need the extra capacity here, edges for every cell are already factored in
    const u32 rightSideTris = height * 6 * 3;
    const u32 topSideTris = width * 6 * 3;
    const u32 triangleCount = (trisPerCell * cellCount) + rightSideTris + topSideTris;
    // assign model data
    size_t vertexDataSize = vertexSize * vertexCount;
    size_t indexDataSize = sizeof(u32) * triangleCount;
    htw_MeshBufferSet modelData = {
        .vertexBuffer = htw_createBuffer(vkContext, bufferPool, vertexDataSize, HTW_BUFFER_USAGE_VERTEX),
        .indexBuffer = htw_createBuffer(vkContext, bufferPool, indexDataSize, HTW_BUFFER_USAGE_INDEX),
        .vertexCount = vertexCount,
        .indexCount = triangleCount,
        .instanceCount = 0
    };
    bc_Mesh chunkMesh = {
        .meshBufferSet = modelData,
        .vertexData = malloc(vertexDataSize),
        .vertexDataSize = vertexDataSize,
        .indexData = malloc(indexDataSize),
        .indexDataSize = indexDataSize
    };
    hexmap->chunkMesh = chunkMesh;

    // hexagon model data
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

    // create and write model data for each cell
    // vert and tri array layout: | main chunk data (left to right, bottom to top) | right chunk edge strip (bottom to top) | top chunk edge strip (left to right) | top-right corner infill |
    bc_HexmapVertexData *vertexData = hexmap->chunkMesh.vertexData;
    u32 *triData = hexmap->chunkMesh.indexData;
    u32 vIndex = 0;
    u32 tIndex = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned int c = x + (y * width); // current cell
            // vertex data
            htw_geo_GridCoord cellCoord = {x, y};
            for (int v = 0; v < vertsPerCell; v++) {
                float posX, posY;
                htw_geo_getHexCellPositionSkewed(cellCoord, &posX, &posY);
                vec3 pos = vec3Add(hexagonPositions[v], (vec3){{posX, posY, 0.0}});
                bc_HexmapVertexData newVertex = {
                    .position = pos,
                    .cellIndex = c
                };
                vertexData[vIndex++] = newVertex;
            }
            // triangle data TODO: skip filling chunk edges with 0's, will use that part of the array later when creating edge strips
            int tBase = 0; // tracks relative tri index for current part of cell mesh
            // central hexagon
            for (int t = tBase; t < trisPerHex; t++) {
                triData[tIndex++] = hexagonIndicies[t] + (c * vertsPerCell);
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
                    triData[tIndex++] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerQuad; t+= 6) {
                    // counter-clockwise from top left
                    unsigned int p1 = 2 + (c * vertsPerCell);
                    unsigned int p2 = 3 + (c * vertsPerCell);
                    unsigned int p3 = 5 + ((c + 1) * vertsPerCell);
                    unsigned int p4 = 6 + ((c + 1) * vertsPerCell);
                    triData[tIndex++] = p1;
                    triData[tIndex++] = p3;
                    triData[tIndex++] = p2;
                    triData[tIndex++] = p1;
                    triData[tIndex++] = p4;
                    triData[tIndex++] = p3;
                }
            }
            tBase += trisPerQuad;
            // top-right edge quad
            if (topEdge) {
                // fill edge with 0s
                for (int t = tBase; t < tBase + trisPerQuad; t++) {
                    triData[tIndex++] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerQuad; t+= 6) {
                    // counter-clockwise from top left
                    unsigned int p1 = 1 + (c * vertsPerCell);
                    unsigned int p2 = 2 + (c * vertsPerCell);
                    unsigned int p3 = 4 + ((c + width) * vertsPerCell);
                    unsigned int p4 = 5 + ((c + width) * vertsPerCell);
                    triData[tIndex++] = p1;
                    triData[tIndex++] = p3;
                    triData[tIndex++] = p2;
                    triData[tIndex++] = p1;
                    triData[tIndex++] = p4;
                    triData[tIndex++] = p3;
                }
            }
            tBase += trisPerQuad;
            // top-left edge quad
            if (leftEdge || topEdge) {
                // fill edge with 0s
                for (int t = tBase; t < tBase + trisPerQuad; t++) {
                    triData[tIndex++] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerQuad; t+= 6) {
                    // counter-clockwise from top left
                    unsigned int p1 = 6 + (c * vertsPerCell);
                    unsigned int p2 = 1 + (c * vertsPerCell);
                    unsigned int p3 = 3 + ((c + (width - 1)) * vertsPerCell);
                    unsigned int p4 = 4 + ((c + (width - 1)) * vertsPerCell);
                    triData[tIndex++] = p1;
                    triData[tIndex++] = p3;
                    triData[tIndex++] = p2;
                    triData[tIndex++] = p1;
                    triData[tIndex++] = p4;
                    triData[tIndex++] = p3;
                }
            }
            tBase += trisPerQuad;
            // top-right corner triangle
            if (rightEdge || topEdge) {
                // fill edge with 0s
                for (int t = tBase; t < tBase + trisPerCorner; t++) {
                    triData[tIndex++] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerCorner; t+= 3) {
                    // clockwise from current hex
                    unsigned int p1 = 2 + (c * vertsPerCell);
                    unsigned int p2 = 4 + ((c + width) * vertsPerCell);
                    unsigned int p3 = 6 + ((c + 1) * vertsPerCell);
                    triData[tIndex++] = p1;
                    triData[tIndex++] = p2;
                    triData[tIndex++] = p3;
                }
            }
            tBase += trisPerCorner;
            // top corner triangle
            if (leftEdge || topEdge) {
                // fill edge with 0s
                for (int t = tBase; t < tBase + trisPerCorner; t++) {
                    triData[tIndex++] = 0;
                }
            }
            else {
                for (int t = tBase; t < tBase + trisPerCorner; t+= 3) {
                    // clockwise from current hex
                    unsigned int p1 = 1 + (c * vertsPerCell);
                    unsigned int p2 = 3 + ((c + (width - 1)) * vertsPerCell);
                    unsigned int p3 = 5 + ((c + width) * vertsPerCell);
                    triData[tIndex++] = p1;
                    triData[tIndex++] = p2;
                    triData[tIndex++] = p3;
                }
            }
        }
    }

    // write model data for edge strips
    // NOTE: 'newVertex.cellIndex' is misleading here, actually corresponds to the terrain buffer index where neighboring chunk data for these verts will be stored
    // NOTE: there is an issue with the top-right corner infill, clicking there will try to access an out of bounds chunk position. TODO: disable cell detection when hovering over a connecting edge
    // starting terrain buffer index for neighboring chunk data
    u32 baseCell = width * height;
    // right edge
    static const u32 leftEdgeConnectionVerts[] = {5, 6, 1};
    for (int y = 0; y < height; y++) {
        u8 isTop = y == height - 1;
        u32 x = width - 1;
        u32 c1 = x + (y * width); // cell to the left of the current cell
        u32 c2 = c1 + width; // cell to the top-left
        u32 c3 = baseCell + y; // current cell
        htw_geo_GridCoord cellCoord = {x + 1, y};
        // vertex data; from bottom-left to top vertex
        for (int v = 0; v < 3; v++) {
            float posX, posY;
            htw_geo_getHexCellPositionSkewed(cellCoord, &posX, &posY);
            u32 hexPosIndex = leftEdgeConnectionVerts[v];
            vec3 pos = vec3Add(hexagonPositions[hexPosIndex], (vec3){{posX, posY, 0.0}});
            bc_HexmapVertexData newVertex = {
                .position = pos,
                .cellIndex = c3
            };
            vertexData[vIndex++] = newVertex;
        }
        // triangle data
        // reference verts run from the bottom up the left edge, then up the right edge
        u32 p1 = 3 + (c1 * vertsPerCell);
        u32 p2 = 2 + (c1 * vertsPerCell);
        u32 p3 = 4 + (c2 * vertsPerCell);
        u32 p4 = 3 + (c2 * vertsPerCell);
        u32 p5 = vIndex - 3;
        u32 p6 = vIndex - 2;
        u32 p7 = vIndex - 1;
        u32 p8 = vIndex;
        // left edge quad
        triData[tIndex++] = p2;
        triData[tIndex++] = p5;
        triData[tIndex++] = p1;
        triData[tIndex++] = p2;
        triData[tIndex++] = p6;
        triData[tIndex++] = p5;
        if (!isTop) {
            // top left corner tri
            triData[tIndex++] = p3;
            triData[tIndex++] = p6;
            triData[tIndex++] = p2;
            // top left edge quad
            triData[tIndex++] = p4;
            triData[tIndex++] = p6;
            triData[tIndex++] = p3;
            triData[tIndex++] = p4;
            triData[tIndex++] = p7;
            triData[tIndex++] = p6;
            // top center corner tri
            triData[tIndex++] = p4;
            triData[tIndex++] = p8;
            triData[tIndex++] = p7;
        }
    }
    u32 lastRightEdgeVertIndex = vIndex - 1; // save to fill the top right corner later
    baseCell += height;
    // top edge + top right corner
    static const u32 topEdgeConnectionVerts[] = {5, 4, 3};
    for (int x = 0; x < width; x++) {
        u8 isRightEdge = x == width - 1;
        u32 y = height - 1;
        u32 c1 = x + (y * width); // cell below the current cell
        u32 c2 = c1 + 1; // cell to the bottom-right
        u32 c3 = baseCell + x; // current cell
        htw_geo_GridCoord cellCoord = {x, y + 1};
        // vertex data; from bottom-left to top vertex
        for (int v = 0; v < 3; v++) {
            float posX, posY;
            htw_geo_getHexCellPositionSkewed(cellCoord, &posX, &posY);
            u32 hexPosIndex = topEdgeConnectionVerts[v];
            vec3 pos = vec3Add(hexagonPositions[hexPosIndex], (vec3){{posX, posY, 0.0}});
            bc_HexmapVertexData newVertex = {
                .position = pos,
                .cellIndex = c3
            };
            vertexData[vIndex++] = newVertex;
        }
        // triangle data
        // reference verts run from the left across the bottom edge, then across from the top edge
        u32 p1 = 1 + (c1 * vertsPerCell);
        u32 p2 = 2 + (c1 * vertsPerCell);
        u32 p3 = 6 + (c2 * vertsPerCell);
        u32 p4 = 1 + (c2 * vertsPerCell);
        u32 p5 = vIndex - 3;
        u32 p6 = vIndex - 2;
        u32 p7 = vIndex - 1;
        u32 p8 = vIndex;
        if (isRightEdge) {
            // add top-right corner vertex
            cellCoord = (htw_geo_GridCoord){x + 1, y + 1};
            float posX, posY;
            htw_geo_getHexCellPositionSkewed(cellCoord, &posX, &posY);
            vec3 pos = vec3Add(hexagonPositions[5], (vec3){{posX, posY, 0.0}});
            bc_HexmapVertexData newVertex = {
                .position = pos,
                .cellIndex = baseCell + width
            };
            vertexData[vIndex++] = newVertex;
            // redefine p3 and p4
            p4 = lastRightEdgeVertIndex;
            p3 = p4 - 1;
        }
        // bottom left edge quad
        triData[tIndex++] = p1;
        triData[tIndex++] = p6;
        triData[tIndex++] = p2;
        triData[tIndex++] = p1;
        triData[tIndex++] = p5;
        triData[tIndex++] = p6;
        // bottom middle corner tri
        triData[tIndex++] = p2;
        triData[tIndex++] = p6;
        triData[tIndex++] = p3;
        // bottom right edge quad
        triData[tIndex++] = p3;
        triData[tIndex++] = p7;
        triData[tIndex++] = p4;
        triData[tIndex++] = p3;
        triData[tIndex++] = p6;
        triData[tIndex++] = p7;
        // bottom right corner tri
        triData[tIndex++] = p4;
        triData[tIndex++] = p7;
        triData[tIndex++] = p8;
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

void bc_writeTerrainBuffers(htw_VkContext *vkContext, bc_RenderableHexmap *hexmap) {
    bc_Mesh chunkMesh = hexmap->chunkMesh;
    htw_writeBuffer(vkContext, chunkMesh.meshBufferSet.vertexBuffer, chunkMesh.vertexData, chunkMesh.vertexDataSize);
    htw_writeBuffer(vkContext, chunkMesh.meshBufferSet.indexBuffer, chunkMesh.indexData, chunkMesh.indexDataSize);
}

void bc_updateHexmapDescriptors(htw_VkContext *vkContext, bc_HexmapTerrain *terrain) {
    htw_updateTerrainObjectDescriptors(vkContext, terrain->chunkObjectDescriptorSets, terrain->terrainBuffer);
}

void bc_updateTerrainVisibleChunks(htw_VkContext *vkContext, htw_ChunkMap *chunkMap, bc_HexmapTerrain *terrain, u32 centerChunk) {
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

    // // TEST
    // printf("closest chunks: ");
    // for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++) {
    //     printf("%i, ", closestChunks[i]);
    // }
    // printf("\n");
    // printf("loaded chunks: ");
    // for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++) {
    //     printf("%i, ", loadedChunks[i]);
    // }
    // printf("\n");

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

    // // TEST
    // printf("closest chunks: ");
    // for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++) {
    //     printf("%i, ", closestChunks[i]);
    // }
    // printf("\n");
    // printf("loaded chunks: ");
    // for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++) {
    //     printf("%i, ", loadedChunks[i]);
    // }
    // printf("\n");

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

    // // TEST
    // printf("loaded chunks: ");
    // for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++) {
    //     printf("%i, ", loadedChunks[i]);
    // }
    // printf("\n\n");

    for (int c = 0; c < terrain->renderedChunkCount; c++) {
        // TODO: only update chunk if freshly visible or has pending updates
        updateHexmapDataBuffer(vkContext, chunkMap, terrain, loadedChunks[c], c);
    }
}

void bc_drawHexmapTerrain(htw_VkContext *vkContext, htw_ChunkMap *chunkMap, bc_RenderableHexmap *hexmap, bc_HexmapTerrain *terrain) {
    htw_bindPipeline(vkContext, hexmap->pipeline);

    static mat4x4 modelMatrix;

    // draw full terrain mesh
    for (int c = 0; c < terrain->renderedChunkCount; c++) {
        u32 chunkIndex = terrain->loadedChunks[c];
        static float chunkX, chunkY;
        htw_geo_getChunkRootPosition(chunkMap, chunkIndex, &chunkX, &chunkY);
        vec3 chunkPos = {{chunkX, chunkY, 0.0}};

        htw_bindDescriptorSet(vkContext, hexmap->pipeline, terrain->chunkObjectDescriptorSets[c], HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_OBJECT);
        mat4x4SetTranslation(modelMatrix, chunkPos);
        htw_drawPipelineX4(vkContext, hexmap->pipeline, &hexmap->chunkMesh.meshBufferSet, HTW_DRAW_TYPE_INDEXED, (float*)modelMatrix);
    }
}

static void updateHexmapDataBuffer (htw_VkContext *vkContext, htw_ChunkMap *chunkMap, bc_HexmapTerrain *terrain, u32 chunkIndex, u32 subBufferIndex) {
    const u32 width = bc_chunkSize;
    const u32 height = bc_chunkSize;
    htw_Chunk *chunk = &chunkMap->chunks[chunkIndex];

    size_t subBufferOffset = subBufferIndex * terrain->terrainBuffer.subBufferHostSize;
    bc_TerrainBufferData *subBuffer = ((void*)terrain->terrainBufferData) + subBufferOffset; // cast to void* so that offset is applied correctly
    subBuffer->chunkIndex = chunkIndex;
    bc_CellData *cellData = &subBuffer->chunkData; // address of cell data array

    // get primary chunk data
    for (s32 y = 0; y < height; y++) {
        for (s32 x = 0; x < width; x++) {
            u32 c = x + (y * width);
            bc_CellData *cell = bc_getCellByIndex(chunkMap, chunkIndex, c);
            cellData[c] = *cell;
            //setHexmapBufferFromCell(cell, &cellData[c]);
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
        //setHexmapBufferFromCell(cell, &cellData[c]);
    }
    edgeDataIndex += height;

    // get chunk data from chunk at y + 1
    u32 topChunk = htw_geo_getChunkIndexAtOffset(chunkMap, chunkIndex, (htw_geo_GridCoord){0, 1});
    chunk = &chunkMap->chunks[topChunk];
    for (s32 x = 0; x < width; x++) {
        u32 c = edgeDataIndex + x;
        bc_CellData *cell = bc_getCellByIndex(chunkMap, topChunk, x);
        cellData[c] = *cell;
        //setHexmapBufferFromCell(cell, &cellData[c]);
    }
    edgeDataIndex += width;

    // get chunk data from chunk at x+1, y+1 (only one cell)
    u32 topRightChunk = htw_geo_getChunkIndexAtOffset(chunkMap, chunkIndex, (htw_geo_GridCoord){1, 1});
    chunk = &chunkMap->chunks[topRightChunk];
    bc_CellData *cell = bc_getCellByIndex(chunkMap, topRightChunk, 0);
    cellData[edgeDataIndex] = *cell;
    //setHexmapBufferFromCell(cell, &cellData[edgeDataIndex]); // set last position in buffer

    htw_writeSubBuffer(vkContext, &terrain->terrainBuffer, subBufferIndex, subBuffer, terrain->terrainBufferSize);
}

static void setHexmapBufferFromCell(bc_CellData *cell, bc_TerrainCellData *target) {
    bc_TerrainCellData cellData = {
        .elevation = cell->height,
        .paletteX = (u8)cell->temperature,
        .paletteY = (u8)cell->nutrient,
        //.paletteX = (u8)remap_int(rawTemperature, 0, chunk->temperatureMap->maxMagnitude, 0, 255),
        //.paletteY = (u8)remap_int(rawRainfall, 0, chunk->rainfallMap->maxMagnitude, 0, 255),
        .visibilityBits = (u8)cell->visibility,
    };
    *target = cellData;

}
