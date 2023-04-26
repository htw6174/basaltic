#include "basaltic_systems_view_terrain.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "components/basaltic_components_planes.h"
#include "basaltic_worldState.h"
#include "htw_core.h"
#include "sokol_gfx.h"
#include "basaltic_sokol_gfx.h"
#define GL_GLEXT_PROTOTYPES
#include "GL/gl.h"

typedef struct {
    vec3 position;
    u16 cellIndex;
    u16 unused;
} bc_HexmapVertexData;

// Compressed version of bc_CellData used for rendering
// TODO: deprecated, only go back to this if video memory starts to become an issue
typedef struct {
    // joined to int packed1
    s16 elevation;
    u8 paletteX;
    u8 paletteY;
    // joined to uint packed2
    u8 visibilityBits;
    u8 lightingBits; // TODO: use one of these bits for solid underground areas? Could override elevation as well to create walls
    u16 unused2; // weather / temporary effect bitmask?
    //int64_t aligner;
} bc_TerrainCellData;

sg_bindings createHexmapBindings(u32 *out_elementCount);
void updateTerrainVisibleChunks(htw_ChunkMap *chunkMap, TerrainBuffer *terrain, u32 centerChunk);
void updateHexmapDataBuffer(htw_ChunkMap *chunkMap, TerrainBuffer *terrain, u32 chunkIndex, u32 subBufferIndex);

/**
 * @brief Creates featureless hexagonal tile surface geometry, detail is added later by shaders
 *
 * @param out_elementCount number of triangle indicies
 */
sg_bindings createHexmapBindings(u32 *out_elementCount) {
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
    *out_elementCount = triangleCount;

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
    // vert and tri array layout: [main chunk data (left to right, bottom to top), right chunk edge strip (bottom to top), top chunk edge strip (left to right), top-right corner infill]
    bc_HexmapVertexData *vertexData = malloc(vertexDataSize);
    u32 *triData = malloc(indexDataSize);
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

    sg_buffer_desc vbd = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .data.ptr = vertexData,
        .data.size = vertexDataSize
    };
    sg_buffer vb = sg_make_buffer(&vbd);

    sg_buffer_desc ibd = {
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data.ptr = triData,
        .data.size = indexDataSize
    };
    sg_buffer ib = sg_make_buffer(&ibd);

    return (sg_bindings){
        .vertex_buffers[0] = vb,
        .index_buffer = ib
    };
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
        bool foundMatch = false;
        for (int k = 0; k < terrain->renderedChunkCount; k++) {
            u32 requiredTarget = closestChunks[k];
            if (requiredTarget != -1 && loadedTarget == requiredTarget) {
                // found a position in a that already contains an element of b
                // mark b[k] -1 so we know it doesn't need to be touched later
                closestChunks[k] = -1;
                foundMatch = true;
                break;
            }
        }
        if (!foundMatch) {
            // a[i] needs to be replaced with an element of b
            loadedChunks[i] = -1;
        }
    }

    // move all positive values from b into negative values of a
    u32 ib = 0;
    for (u32 ia = 0; ia < terrain->renderedChunkCount; ia++) {
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
    }

    for (int c = 0; c < terrain->renderedChunkCount; c++) {
        static float chunkX, chunkY;
        htw_geo_getChunkRootPosition(chunkMap, loadedChunks[c], &chunkX, &chunkY);
        terrain->chunkPositions[c] = (vec3){{chunkX, chunkY, 0.0}};
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

    sg_shader_uniform_block_desc fsdLocal = {
        .size = sizeof(s32),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "chunkIndex", .type = SG_UNIFORMTYPE_INT}
        }
    };

    sg_shader_desc tsd = {
        .vs.uniform_blocks[0] = vsdGlobal,
        .vs.uniform_blocks[1] = vsdLocal,
        .vs.source = vert,
        .fs.uniform_blocks[0] = fsdGlobal,
        .fs.uniform_blocks[1] = fsdLocal,
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
    sg_bindings bind = createHexmapBindings(&elementCount);

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

        s32 *loadedChunks = calloc(renderedChunkCount, sizeof(u32));
        // Initialize loaded chunks to those closest to the origin. TODO: this is related to an issue where initially filling loaded chunks takes several calls to updateTerrainVisibleChunks. This fixes the issue when renderedChunkCount == total chunk count
        for (int c = 0; c < renderedChunkCount; c++) {
            loadedChunks[c] = c;
        }

        ecs_set(it->world, it->entities[i], TerrainBuffer, {
            .gluint = terrainBuffer,
            .renderedChunkRadius = visibilityRadius,
            .renderedChunkCount = renderedChunkCount,
            .closestChunks = calloc(renderedChunkCount, sizeof(u32)),
            .loadedChunks = loadedChunks,
            .chunkPositions = calloc(renderedChunkCount, sizeof(vec3)),
            .chunkBufferCellCount = chunkBufferCellCount,
            .data = malloc(bufferSize)
        });
    }

    sg_reset_state_cache();
}

void UpdateHexTerrainBuffers(ecs_iter_t *it) {
    Binding *binds = ecs_field(it, Binding, 1);
    ModelQuery *queries = ecs_field(it, ModelQuery, 2);
    TerrainBuffer *terrainBuffers = ecs_field(it, TerrainBuffer, 3);

    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;

    for (int i = 0; i < it->count; i++) {
        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Plane *planes = ecs_field(&mit, Plane, 1);

            for (int m = 0; m < mit.count; m++) {
                htw_ChunkMap *cm = planes[m].chunkMap;

                // TODO: correctly determine center chunk from camera focus
                u32 centerChunk = bc_getChunkIndexByWorldPosition(cm, 0.0f, 0.0f);
                updateTerrainVisibleChunks(cm, &terrainBuffers[i], centerChunk);
            }
        }
    }
}

void DrawPipelineHexTerrain(ecs_iter_t *it) {
    Pipeline *pips = ecs_field(it, Pipeline, 1);
    Binding *binds = ecs_field(it, Binding, 2);
    TerrainBuffer *terrainBuffers = ecs_field(it, TerrainBuffer, 3);
    PVMatrix *pvs = ecs_field(it, PVMatrix, 4);
    Mouse *mouse = ecs_field(it, Mouse, 5);
    FeedbackBuffer *feedback = ecs_field(it, FeedbackBuffer, 6);
    HoveredCell *hovered = ecs_field(it, HoveredCell, 7);

    const WrapInstanceOffsets *wraps = ecs_singleton_get(it->world, WrapInstanceOffsets);

    for (int i = 0; i < it->count; i++) {
        sg_apply_pipeline(pips[i].pipeline);
        sg_apply_bindings(&binds[i].binding);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));
        sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &SG_RANGE(mouse[i]));

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, feedback[i].gluint);

        for (int c = 0; c < terrainBuffers[i].renderedChunkCount; c++) {
            u32 chunkIndex = terrainBuffers[i].loadedChunks[c];
            sg_apply_uniforms(SG_SHADERSTAGE_FS, 1, &SG_RANGE(chunkIndex));

            size_t range = terrainBuffers[i].chunkBufferCellCount * sizeof(bc_CellData);
            size_t offset = c * range;
            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, terrainBuffers[i].gluint, offset, range);

            //if (glGetError()) printf("%x\n", glGetError());

            if (wraps != NULL) {
                bc_drawWrapInstances(0, binds[i].elements, 1, 1, terrainBuffers[i].chunkPositions[c], wraps->offsets);
            } else {
                mat4x4 model;
                mat4x4SetTranslation(model, terrainBuffers[i].chunkPositions[c]);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 1, &SG_RANGE(model));
                sg_draw(0, binds[i].elements, 1);
            }
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, feedback[i].gluint);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(hovered[i]), &hovered[i]);
    }
}

void BasalticSystemsViewTerrainImport(ecs_world_t *world) {
    ECS_MODULE(world, BasalticSystemsViewTerrain);

    ECS_IMPORT(world, BasalticComponentsView);
    ECS_IMPORT(world, BasalticPhasesView);
    ECS_IMPORT(world, BasalticComponentsPlanes);

    ECS_SYSTEM(world, SetupPipelineHexTerrain, EcsOnStart,
               [in] RenderDistance($),
               [out] !Pipeline,
               [out] !Binding,
               [out] !QueryDesc,
               [out] !TerrainBuffer,
               [none] basaltic.components.view.TerrainRender,
    );

    ECS_SYSTEM(world, UpdateHexTerrainBuffers, OnModelChanged,
               [in] Binding,
               [in] ModelQuery,
               [inout] TerrainBuffer,
               [none] ModelWorld($),
               [none] basaltic.components.view.TerrainRender,
    );

    ECS_SYSTEM(world, DrawPipelineHexTerrain, EcsOnUpdate,
               [in] Pipeline,
               [in] Binding,
               [in] TerrainBuffer,
               [in] PVMatrix($),
               [in] Mouse($),
               [in] FeedbackBuffer($),
               [out] HoveredCell($),
               [none] ModelWorld($),
               [none] WrapInstanceOffsets($),
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
}
