#include "basaltic_systems_view_terrain.h"
#include "basaltic_components_view.h"
#include "basaltic_phases_view.h"
#include "components/basaltic_components_planes.h"
#include "basaltic_worldState.h"
#include "htw_core.h"
#include "sokol_gfx.h"
#include "basaltic_sokol_gfx.h"
#ifdef _WIN32
#include <GL/glew.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#endif

typedef struct {
    vec2 position;
    float neighborWeight;
    vec3 barycentric;
    s16 localX;
    s16 localY;
} hexmapVertexData;

typedef struct {
    vec4 position;
    vec2 rootCoord;
} chunkInstanceData;

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
} terrainCellData;

static sg_shader_desc terrainShadowShaderDescription = {
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
    .vs.images[0] = {
        .used = true,
        .image_type = SG_IMAGETYPE_2D,
        .sample_type = SG_IMAGESAMPLETYPE_SINT,
    },
    .vs.samplers[0] = {
        .used = true,
        .sampler_type = SG_SAMPLERTYPE_NONFILTERING,
    },
    .vs.image_sampler_pairs[0] = {
        .used = true,
        .image_slot = 0,
        .sampler_slot = 0,
        .glsl_name = "terrain"
    },
    .vs.source = NULL,
    .fs.source = NULL
};

static sg_pipeline_desc terrainShadowPipelineDescription = {
    .shader = {0},
    .layout = {
        .buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
        .attrs = {
            [0] = {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT4},
            [1] = {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT2},
            [2] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT2},
            [3] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT},
            [4] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT3},
            [5] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_SHORT2N}
        }
    },
    .index_type = SG_INDEXTYPE_UINT32,
    .depth = {
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true,
        .bias = 0.01f,
        .bias_slope_scale = 1.0f
    },
    .cull_mode = SG_CULLMODE_BACK,
    .sample_count = 1,
    // important: 'deactivate' the default color target for 'depth-only-rendering'
    .colors[0].pixel_format = SG_PIXELFORMAT_NONE
};

// NOTE: can eliminate most of the shader spec later when using reflection utils
static sg_shader_desc terrainShaderDescription = {
    .vs.uniform_blocks[0] = {
        .size = sizeof(PVMatrix),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "pv", .type = SG_UNIFORMTYPE_MAT4}
        }
    },
    .vs.uniform_blocks[1] = {
        .size = sizeof(ModelMatrix),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "m", .type = SG_UNIFORMTYPE_MAT4}
        }
    },
    .vs.images[0] = {
        .used = true,
        .image_type = SG_IMAGETYPE_2D,
        .sample_type = SG_IMAGESAMPLETYPE_SINT,
    },
    .vs.samplers[0] = {
        .used = true,
        .sampler_type = SG_SAMPLERTYPE_NONFILTERING,
    },
    .vs.image_sampler_pairs[0] = {
        .used = true,
        .image_slot = 0,
        .sampler_slot = 0,
        .glsl_name = "terrain"
    },
    .vs.source = NULL,
    .fs.uniform_blocks[0] = {
        .size = sizeof(float),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "time", .type = SG_UNIFORMTYPE_FLOAT}
        }
    },
    .fs.uniform_blocks[1] = {
        // NOTE: when using STD140, vec4 size is required here, even though the size/alignment requirement should only be 8 bytes for a STD140 vec2 uniform. Maybe don't bother with STD140 since I only plan to use the GL backend. Can add sokol-shdc to the project later to improve cross-platform support if I really need it.
        .size = sizeof(vec2),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "mousePosition", .type = SG_UNIFORMTYPE_FLOAT2}
        }
    },
    .fs.uniform_blocks[2] = {
        .size = sizeof(s32),
        .layout = SG_UNIFORMLAYOUT_NATIVE,
        .uniforms = {
            [0] = {.name = "chunkIndex", .type = SG_UNIFORMTYPE_INT}
        }
    },
    .fs.source = NULL
};

static sg_pipeline_desc terrainPipelineDescription = {
    .shader = {0}, // TODO: setup a fallback shader here
    .layout = {
        .buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
        .attrs = {
            [0] = {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT4},
            [1] = {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT2},
            [2] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT2},
            [3] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT},
            [4] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT3},
            [5] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_SHORT2N}
        }
    },
    .index_type = SG_INDEXTYPE_UINT32,
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
        [2].pixel_format = SG_PIXELFORMAT_RG16SI,
    }
};

vec3 barycentric(vec2 p, vec2 left, vec2 right);
vec3 getHexVertBarycentric(vec2 pos, int neighborhoodIndex);

Mesh createHexmapMesh(void);
Mesh createTriGridMesh(u32 width, u32 height, u32 subdivisions);
void updateTerrainVisibleChunks(Plane *plane, TerrainBuffer *terrain, DataTexture *dataTexture, u32 centerChunk);

void updateDataTextureChunk(Plane *plane, DataTexture *dataTexture, u32 chunkIndex);

void InitTerrainInstances(ecs_iter_t *it);
void UpdateTerrainInstances(ecs_iter_t *it);

void InitTerrainDataTexture(ecs_iter_t *it);
void UpdateTerrainDataTexture(ecs_iter_t *it);
void UpdateTerrainDataTextureDirtyChunks(ecs_iter_t *it);

// could use simpler 2d only version but w/e
vec3 barycentric(vec2 p, vec2 left, vec2 right) {
    //vec2 v0 = vec2Subtract(b, a), v1 = vec2Subtract(c, a), v2 = vec2Subtract(p, a);
    vec2 v0 = left, v1 = right, v2 = p;
    float d00 = vec2DotProduct(v0, v0);
    float d01 = vec2DotProduct(v0, v1);
    float d11 = vec2DotProduct(v1, v1);
    float d20 = vec2DotProduct(v2, v0);
    float d21 = vec2DotProduct(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;
    return (vec3){{u, v, w}};
}

vec3 getHexVertBarycentric(vec2 pos, int neighborhoodIndex) {
    float x1, y1, x2, y2;
    htw_geo_GridCoord leftCoord = htw_geo_hexGridDirections[(neighborhoodIndex + 5) % HEX_DIRECTION_COUNT];
    htw_geo_GridCoord rightCoord = htw_geo_hexGridDirections[neighborhoodIndex];
    htw_geo_getHexCellPositionSkewed(leftCoord, &x1, &y1);
    htw_geo_getHexCellPositionSkewed(rightCoord, &x2, &y2);
    return barycentric(pos, (vec2){{x1, y1}}, (vec2){{x2, y2}});
}

Mesh createTriGridMesh(u32 width, u32 height, u32 subdivisions) {

    // Setup
    const u32 cellCount = width * height;

    // FIXME: temp fix to fill in gaps. Generate all verts for extra cells to the left, right, bottom. Slightly wasteful but keeps generation algo simple
    // NOTE: *BEWARE* of trying to compare a negative signed int with an unsigned int; -1 < (unsigned int)1 == FALSE
    const s32 expandWidth = width + 2;
    const s32 expandHeight = height + 1;
    const s32 expandedCellCount = expandWidth * expandHeight;

    // Extra 2 per cell along the bottom edge, extra 1 per cell along the left and right edges
    //const u32 edgeVertexCount = 2 * (width + height);
    //const u32 edgeSubdivVertCount = edgeVertexCount * pow(4, subdivisions); // TODO: not sure about this formula
    // is it really as simple as 3 * pow(4, subdiv)? Makes sense in the infinite plane case: 6 tris around every vert, 3 verts for every tri -> verts = tris / 2
    const u32 vertsPerHex = 3 * pow(4, subdivisions);
    const u32 vertexCount = expandedCellCount * vertsPerHex;
    // 6 base tris, each subdivision splits each tri in 4
    const u32 trisPerHex = 6 * pow(4, subdivisions);
    const u32 triangleCount = trisPerHex * cellCount;
    const u32 elesPerHex = trisPerHex * 3;
    const u32 elementCount = elesPerHex * cellCount;

    // useful constants; based on distance between 2 adjacent hex centers == 1
    static const float outerRadius = 0.57735026919; // = sqrt(0.75) * (2.0 / 3.0);
    static const float innerRadius = 0.5;
    static const float halfEdge = outerRadius / 2;
    static const vec2 hexagonPositions[] = {
        { {0.0, 0.0} }, // center
        { {0.0, outerRadius} }, // top middle, runs clockwise
        { {innerRadius, halfEdge} },
        { {innerRadius, -halfEdge} },
        { {0.0, -outerRadius} },
        { {-innerRadius, -halfEdge} },
        { {-innerRadius, halfEdge} },
    };

    // TEST
    printf("Creating tri grid mesh:\n- %u subdivisions\n- %u verts\n- %u tris\n", subdivisions, vertexCount, triangleCount);

    size_t vertexBufferSize = vertexCount * sizeof(hexmapVertexData);
    size_t elementBufferSize = elementCount * sizeof(u32);
    hexmapVertexData *verts = calloc(1, vertexBufferSize);
    u32 *elements = calloc(1, elementBufferSize);

    // vertex data
    u32 vIndex = 0;
    // NOTE: because starting from a negative value, be sure to use signed int for comparison
    for (int y = -1; y < expandHeight - 1; y++) {
        for (int x = -1; x < expandWidth - 1; x++) {
            // setup
            htw_geo_GridCoord cellCoord = {x, y};
            float posX, posY;
            htw_geo_getHexCellPositionSkewed(cellCoord, &posX, &posY);
            vec2 cellPos = (vec2){{posX, posY}};
            // create 3 new verts per cell for the 'home tri'
            for (int v = 0; v < 3; v++) {
                vec2 pos = hexagonPositions[v];
                int neighborhood = MAX(0, v - 1);
                hexmapVertexData newVertex = {
                    .position = vec2Add(pos, cellPos),
                    .neighborWeight = neighborhood,
                    .barycentric = getHexVertBarycentric(pos, neighborhood),
                    .localX = x,
                    .localY = y
                };
                verts[vIndex++] = newVertex;
            }
            // create vert in the center of each edge
            // lines emerging from center:
            for (int v = 0; v < 6; v++) {
                vec2 pos1 = hexagonPositions[0];
                vec2 pos2 = hexagonPositions[v + 1];
                vec2 pos = vec2Mix(pos1, pos2, 0.5);
                int neighborhood = v;
                hexmapVertexData newVertex = {
                    .position = vec2Add(pos, cellPos),
                    .neighborWeight = neighborhood,
                    .barycentric = getHexVertBarycentric(pos, neighborhood),
                    .localX = x,
                    .localY = y
                };
                verts[vIndex++] = newVertex;
            }
            // 'owned' outside edges
            u32 edgeIndicies[] = {6, 1, 2, 3};
            for (int v = 0; v < 3; v++) {
                vec2 pos1 = hexagonPositions[edgeIndicies[v]];
                vec2 pos2 = hexagonPositions[edgeIndicies[v + 1]];
                vec2 pos = vec2Mix(pos1, pos2, 0.5);
                int neighborhood = edgeIndicies[v] - 1;
                hexmapVertexData newVertex = {
                    .position = vec2Add(pos, cellPos),
                    .neighborWeight = neighborhood,
                    .barycentric = getHexVertBarycentric(pos, neighborhood),
                    .localX = x,
                    .localY = y
                };
                verts[vIndex++] = newVertex;
            }
            //assert(vIndex % 12 == 0);
        }
    }
    assert(vIndex == vertexCount);

    // elements
    // TODO: need to handle first row & column. For now just start at cell [1, 1]
    // TODO: need to handle last column (otherwise can't reference SE cell). For now just end row early
    //u32 cellBaseElementIndex = vertsPerHex * (expandWidth + 1);
    u32 eIndex = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            //u32 c = x + (y * width); // current cell index
            // 'vertex cell': must account for the fact that the vertex array is effectively w+2 * h+1, and offset [-1, -1]
            // [0, 0] -> [1, 1], c = 0 -> vc = 1 + (1 * expandWidth)
            u32 vc = (x + 1) + ((y + 1) * expandWidth);
            u32 cellBaseElementIndex = vertsPerHex * vc;
            // get relative element index for west, south-west, and south-east cell
            s32 cbW = -vertsPerHex;
            s32 cbSW = -(vertsPerHex * expandWidth);
            s32 cbSE = cbSW + vertsPerHex;
            // All relevent element indicies for each hextant. Duplicating some indicies makes it easier to iterate over
            s32 basisEles[] = {
                0, 1, 2, 3, 10, 4,
                0, 2, cbSE + 1, 4, 11, 5,
                0, cbSE + 1, cbSW + 2, 5, cbSE + 9, 6,
                0, cbSW + 2, cbSW + 1, 6, cbSW + 10, 7,
                0, cbSW + 1, cbW + 2, 7, cbW + 11, 8,
                0, cbW + 2, 1, 8, 9, 3
            };
            // Index into a row of basisEles
            // NOTE: ordering here is important to ensure the triggering vertex for non-interpolated cell data is correct. Keeping the last vertex inside the hex ensures this, at least for OpenGL
            u32 hextantRelEles[] = {0, 3, 5, 1, 4, 3, 4, 2, 5, 3, 4, 5};
            // Iterate over hextants
            for (int h = 0; h < 6; h++) {
                for (int e = 0; e < 12; e++) {
                    u32 ele = cellBaseElementIndex + basisEles[hextantRelEles[e] + (6 * h)];
                    //assert(ele > 0 && ele < vertexCount);
                    elements[eIndex++] = ele;
                }
            }
            //cellBaseElementIndex += vertsPerHex;
        }
    }
    assert(eIndex == elementCount);

    // Vertex and index creation should be sort of separate; element indexes are not mostly local any more, need to find and use verts mostly from nearby cells

    sg_buffer_desc vbd = {
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .data.ptr = verts,
        .data.size = vertexBufferSize
    };
    sg_buffer vb = sg_make_buffer(&vbd);

    sg_buffer_desc ibd = {
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data.ptr = elements,
        .data.size = elementBufferSize
    };
    sg_buffer ib = sg_make_buffer(&ibd);

    free(verts);
    free(elements);

    return (Mesh) {
        .vertexBuffers[0] = vb,
        .indexBuffer = ib,
        .elements = elementCount
    };
}

/**
 * @brief Creates featureless hexagonal tile surface geometry, detail is added later by shaders
 *
 * @param out_elementCount number of triangle indicies
 */
Mesh createHexmapMesh(void) {
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
    const u32 vertexSize = sizeof(hexmapVertexData);
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

    // hexagon model data
    static const float halfHeight = 0.433012701892;
    static const vec2 hexagonPositions[] = {
        { {0.0, 0.0} }, // center
        { {0.0, 0.5} }, // top middle, runs clockwise
        { {halfHeight, 0.25} },
        { {halfHeight, -0.25} },
        { {0.0, -0.5} },
        { {-halfHeight, -0.25} },
        { {-halfHeight, 0.25} },
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
    hexmapVertexData *vertexData = malloc(vertexDataSize);
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
                vec2 pos = hexagonPositions[v];
                int neighborhood = MAX(0, v - 1);
                hexmapVertexData newVertex = {
                    .position = vec2Add(pos, (vec2){{posX, posY}}),
                    .neighborWeight = neighborhood,
                    .barycentric = getHexVertBarycentric(pos, neighborhood),
                    .localX = x,
                    .localY = y
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
            vec2 pos = hexagonPositions[hexPosIndex];
            hexmapVertexData newVertex = {
                .position = vec2Add(pos, (vec2){{posX, posY}}),
                .neighborWeight = hexPosIndex - 1,
                .barycentric = getHexVertBarycentric(pos, hexPosIndex - 1),
                .localX = x + 1,
                .localY = y
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
            vec2 pos = hexagonPositions[hexPosIndex];
            hexmapVertexData newVertex = {
                .position = vec2Add(pos, (vec2){{posX, posY}}),
                .neighborWeight = hexPosIndex - 1,
                .barycentric = getHexVertBarycentric(pos, hexPosIndex - 1),
                .localX = x,
                .localY = y + 1
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
            vec2 pos = hexagonPositions[5];
            hexmapVertexData newVertex = {
                .position = vec2Add(pos, (vec2){{posX, posY}}),
                .neighborWeight = 5 - 1,
                .barycentric = getHexVertBarycentric(pos, 5 - 1),
                .localX = x + 1,
                .localY = y + 1
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

    free(vertexData);
    free(triData);

    return (Mesh) {
        .vertexBuffers[0] = vb,
        .indexBuffer = ib,
        .elements = triangleCount
    };
}

void updateTerrainVisibleChunks(Plane *plane, TerrainBuffer *terrain, DataTexture *dataTexture, u32 centerChunk) {
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

    htw_ChunkMap *chunkMap = plane->chunkMap;
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
        updateDataTextureChunk(plane, dataTexture, loadedChunks[c]);
    }
}

void updateDataTextureChunk(Plane *plane, DataTexture *dataTexture, u32 chunkIndex) {
    const u32 width = bc_chunkSize;
    const u32 height = bc_chunkSize;
    htw_ChunkMap *chunkMap = plane->chunkMap;

    // Find bottom-left corner of image region to update
    htw_geo_GridCoord startTexel = htw_geo_chunkAndCellToGridCoordinates(chunkMap, chunkIndex, 0);
    // Convert texel coord to data buffer location
    size_t texelIndex = (startTexel.y * dataTexture->width) + startTexel.x;
    void *texHead = dataTexture->data + (texelIndex * dataTexture->formatSize);

    // get primary chunk data
    for (s32 y = 0; y < height; y++) {
        for (s32 x = 0; x < width; x++) {
            u32 c = x + (y * width);
            CellData *cell = bc_getCellByIndex(chunkMap, chunkIndex, c);
            /* Info the shader needs:
             * - height
             * - geology
             * - biotemperature
             * - humidity provence
             * - understory coverage
             * - canopy coverage
             * - surface water details
             * - visibility
             */
            s32 biotemp = plane_GetCellBiotemperature(plane, htw_geo_addGridCoords(startTexel, (htw_geo_GridCoord){x, y}));
            u16 tempIndex = remap_int(biotemp, -3000, 3000, 0, 255);

            s32 rChannel = ((u32)cell->geology << 16) + (u16)cell->height; // base terrain
            u32 gChannel = ((u32)cell->humidityPreference << 16) + (u16)tempIndex; // life zone
            u32 bChannel = ((u32)cell->canopy << 16) + (u16)cell->understory; // vegetation
            u32 aChannel = ((u32)cell->visibility << 16) + (u16)0; // water and visibility TODO
            s32 texelData[4] = {
                rChannel,
                gChannel,
                bChannel,
                aChannel
            };
            memcpy(texHead + (x * dataTexture->formatSize), &texelData, sizeof(texelData));
        }
        // Advance to next row of texture
        texHead += dataTexture->width * dataTexture->formatSize;
    }
}

void InitTerrainBuffers(ecs_iter_t *it) {
    RenderDistance *rd = ecs_field(it, RenderDistance, 1);

    for (int i = 0; i < it->count; i++) {
        ecs_set(it->world, it->entities[i], QueryDesc, {
            .expr = "[in] bc.planes.Plane"
        });

        u32 visibilityRadius = rd[i].radius;
        u32 visibilityDiameter = (visibilityRadius * 2) - 1;
        u32 renderedChunkCount = visibilityDiameter * visibilityDiameter;

        s32 *loadedChunks = calloc(renderedChunkCount, sizeof(u32));
        // Initialize loaded chunks to those closest to the origin. TODO: this is related to an issue where initially filling loaded chunks takes several calls to updateTerrainVisibleChunks. This fixes the issue when renderedChunkCount == total chunk count
        for (int c = 0; c < renderedChunkCount; c++) {
            loadedChunks[c] = c;
        }

        // TODO: consider replacing entirely with data texture
        ecs_set(it->world, it->entities[i], TerrainBuffer, {
            .renderedChunkRadius = visibilityRadius,
            .renderedChunkCount = renderedChunkCount,
            .closestChunks = calloc(renderedChunkCount, sizeof(u32)),
                .loadedChunks = loadedChunks,
                .chunkPositions = calloc(renderedChunkCount, sizeof(vec3)),
        });
    }
}

void UpdateHexTerrainBuffers(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);
    TerrainBuffer *terrainBuffers = ecs_field(it, TerrainBuffer, 2);
    DataTexture *dataTextures = ecs_field(it, DataTexture, 3);

    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;

    for (int i = 0; i < it->count; i++) {
        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Plane *planes = ecs_field(&mit, Plane, 1);

            for (int m = 0; m < mit.count; m++) {
                htw_ChunkMap *cm = planes[m].chunkMap;

                // TODO: correctly determine center chunk from camera focus
                u32 centerChunk = bc_getChunkIndexByWorldPosition(cm, 0.0f, 0.0f);
                updateTerrainVisibleChunks(&planes[m], &terrainBuffers[i], &dataTextures[i], centerChunk);
            }
        }
    }
}

void InitTerrainInstances(ecs_iter_t *it) {
    size_t instanceDataSize = sizeof(chunkInstanceData) * 64 * 64; // TODO: for terrain, size of instance buffer should be based on number of rendered chunks

    for (int i = 0; i < it->count; i++) {
        // TODO: Make component destructor to free memory
        chunkInstanceData *instanceData = malloc(instanceDataSize);

        sg_buffer_desc vbd = {
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_DYNAMIC,
            .size = instanceDataSize
        };
        sg_buffer instanceBuffer = sg_make_buffer(&vbd);

        ecs_set(it->world, it->entities[i], InstanceBuffer, {.buffer = instanceBuffer, .size = instanceDataSize, .data = instanceData});
    }
}

void UpdateTerrainInstances(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);
    InstanceBuffer *instanceBuffers = ecs_field(it, InstanceBuffer, 2);

    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;

    for (int i = 0; i < it->count; i++) {
        chunkInstanceData *instanceData = instanceBuffers[i].data;
        u32 instanceCount = 0;

        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Plane *planes = ecs_field(&mit, Plane, 1);
            for (int m = 0; m < mit.count; m++) {
                htw_ChunkMap *cm = planes[m].chunkMap;
                for (int c = 0; c < (cm->chunkCountX * cm->chunkCountY); c++) {
                    float x, y;
                    htw_geo_getChunkRootPosition(cm, c, &x, &y);
                    htw_geo_GridCoord rootCoord = htw_geo_chunkAndCellToGridCoordinates(cm, c, 0);
                    instanceData[instanceCount] = (chunkInstanceData){
                        .position = {{x, y, 0.0, 1.0}},
                        .rootCoord = {{rootCoord.x, rootCoord.y}}
                    };
                    instanceCount++;
                }
            }
        }
        instanceBuffers[i].instances = instanceCount;
        sg_update_buffer(instanceBuffers[i].buffer, &(sg_range){.ptr = instanceData, .size = instanceBuffers[i].size});
    }
}

void InitTerrainDataTexture(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);

    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;

    for (int i = 0; i < it->count; i++) {
        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Plane *planes = ecs_field(&mit, Plane, 1);
            for (int m = 0; m < mit.count; m++) {
                htw_ChunkMap *cm = planes[m].chunkMap;

                u32 texWidth = bc_chunkSize * cm->chunkCountX;
                u32 texHeight = bc_chunkSize * cm->chunkCountY;

                sg_image_desc id = {
                    .width = texWidth,
                    .height = texHeight,
                    .usage = SG_USAGE_DYNAMIC,
                    .pixel_format = SG_PIXELFORMAT_RGBA32SI,
                };

                sg_sampler_desc sd = {
                    .min_filter = SG_FILTER_NEAREST,
                    .mag_filter = SG_FILTER_NEAREST
                };

                size_t formatSize = sizeof(s32[4]);
                size_t texSize = texWidth * texHeight * formatSize;
                void *texHostData = malloc(texSize);

                ecs_set(it->world, it->entities[i], DataTexture, {sg_make_image(&id), sg_make_sampler(&sd), texWidth, texHeight, texHostData, texSize, formatSize});
            }
        }
    }
}

void DestroyDataTextures(ecs_iter_t *it) {
    DataTexture *dataTextures = ecs_field(it, DataTexture, 1);

    for (int i = 0; i < it->count; i++) {
        DataTexture dt = dataTextures[i];
        sg_destroy_image(dt.image);
        sg_destroy_sampler(dt.sampler);
        free(dt.data);
        ecs_remove(it->world, it->entities[i], DataTexture);
    }
}

// TODO need to refine the structure here; either each terrain renderer should correspond to exactly one Plane, or they need a way to generalize to several Planes (=several data textures)
void UpdateTerrainDataTexture(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);
    DataTexture *dataTextures = ecs_field(it, DataTexture, 2);

    ecs_world_t *modelWorld = ecs_singleton_get(it->world, ModelWorld)->world;

    for (int i = 0; i < it->count; i++) {
        ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
        while (ecs_query_next(&mit)) {
            Plane *planes = ecs_field(&mit, Plane, 1);
            for (int m = 0; m < mit.count; m++) {
                htw_ChunkMap *cm = planes[m].chunkMap;
                DataTexture dt = dataTextures[i];
                for (int c = 0; c < (cm->chunkCountX * cm->chunkCountY); c++) {
                    updateDataTextureChunk(&planes[m], &dt, c);
                }

                sg_update_image(dt.image, &(sg_image_data){.subimage[0][0] = {dt.data, dt.size}});
            }
        }
    }
}

void UpdateTerrainDataTextureDirtyChunks(ecs_iter_t *it) {
    ModelQuery *queries = ecs_field(it, ModelQuery, 1);
    DataTexture *dataTextures = ecs_field(it, DataTexture, 2);
    DirtyChunkBuffer *dirty = ecs_field(it, DirtyChunkBuffer, 3);
    // singleton
    ecs_world_t *modelWorld = ecs_field(it, ModelWorld, 4)->world;

    for (int i = 0; i < it->count; i++) {
        if (dirty[i].count > 0) {
            ecs_iter_t mit = ecs_query_iter(modelWorld, queries[i].query);
            while (ecs_query_next(&mit)) {
                Plane *planes = ecs_field(&mit, Plane, 1);
                for (int m = 0; m < mit.count; m++) {
                    DataTexture dt = dataTextures[i];
                    for (int c = 0; c < (dirty[i].count); c++) {
                        updateDataTextureChunk(&planes[m], &dt, dirty[i].chunks[c]);
                    }
                    sg_update_image(dt.image, &(sg_image_data){.subimage[0][0] = {dt.data, dt.size}});
                }
            }
        }
        dirty[i].count = 0;
    }
}

// TODO: restrict number of instances drawn, add LOD meshes and draw different instances for each LOD
void DrawHexTerrainShadows(ecs_iter_t *it) {
    int f = 0;
    Pipeline *pips = ecs_field(it, Pipeline, ++f);
    InstanceBuffer *instanceBuffers = ecs_field(it, InstanceBuffer, ++f);
    Mesh *mesh = ecs_field(it, Mesh, ++f);
    DataTexture *dataTextures = ecs_field(it, DataTexture, ++f);
    SunMatrix *sun = ecs_field(it, SunMatrix, ++f);

    const WrapInstanceOffsets *wraps = ecs_singleton_get(it->world, WrapInstanceOffsets);

    for (int i = 0; i < it->count; i++) {
        sg_bindings bind = {
            .vertex_buffers[0] = instanceBuffers[i].buffer,
            .vertex_buffers[1] = mesh[i].vertexBuffers[0],
            .index_buffer = mesh[i].indexBuffer,
            .vs.images[0] = dataTextures[i].image,
            .vs.samplers[0] = dataTextures[i].sampler
        };

        sg_apply_pipeline(pips[i].pipeline);
        sg_apply_bindings(&bind);

        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(sun[i]));

        if (wraps != NULL) {
            bc_drawWrapInstances(0, mesh[i].elements, instanceBuffers[i].instances, 1, (vec3){{0.0, 0.0, 0.0}}, wraps->offsets);
        } else {
            mat4x4 model;
            mat4x4SetTranslation(model, (vec3){{0.0, 0.0, 0.0}});
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 1, &SG_RANGE(model));
            sg_draw(0, mesh[i].elements, instanceBuffers[i].instances);
        }
    }
}

// TODO: restrict number of instances drawn, add LOD meshes and draw different instances for each LOD
void DrawPipelineHexTerrain(ecs_iter_t *it) {
    int f = 0;
    Pipeline *pips = ecs_field(it, Pipeline, ++f);
    InstanceBuffer *instanceBuffers = ecs_field(it, InstanceBuffer, ++f);
    Mesh *mesh = ecs_field(it, Mesh, ++f);
    DataTexture *dataTextures = ecs_field(it, DataTexture, ++f);
    RingBuffer *pixelPack = ecs_field(it, RingBuffer, ++f);
    PVMatrix *pvs = ecs_field(it, PVMatrix, ++f);
    Mouse *mouse = ecs_field(it, Mouse, ++f);
    Clock *programClock = ecs_field(it, Clock, ++f);
    //HoveredCell *hovered = ecs_field(it, HoveredCell, ++f); // may use as uniform input later

    const WrapInstanceOffsets *wraps = ecs_singleton_get(it->world, WrapInstanceOffsets);

    for (int i = 0; i < it->count; i++) {
        sg_bindings bind = {
            .vertex_buffers[0] = instanceBuffers[i].buffer,
            .vertex_buffers[1] = mesh[i].vertexBuffers[0],
            .index_buffer = mesh[i].indexBuffer,
            .vs.images[0] = dataTextures[i].image,
            .vs.samplers[0] = dataTextures[i].sampler
        };

        sg_apply_pipeline(pips[i].pipeline);
        sg_apply_bindings(&bind);

        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(pvs[i]));
        sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &SG_RANGE(programClock[i]));
        sg_apply_uniforms(SG_SHADERSTAGE_FS, 1, &SG_RANGE(mouse[i]));

        if (wraps != NULL) {
            bc_drawWrapInstances(0, mesh[i].elements, instanceBuffers[i].instances, 1, (vec3){{0.0, 0.0, 0.0}}, wraps->offsets);
        } else {
            mat4x4 model;
            mat4x4SetTranslation(model, (vec3){{0.0, 0.0, 0.0}});
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 1, &SG_RANGE(model));
            sg_draw(0, mesh[i].elements, instanceBuffers[i].instances);
        }

        glReadBuffer(GL_COLOR_ATTACHMENT2);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pixelPack[i].writableBuffer);
        // NOTE: readpixels is picky about format and type combos. Even though the framebuffer format is RG16I, need to read it using this combo which returns 4 ints
        glReadPixels(mouse[i].x, mouse[i].y, 1, 1, GL_RGBA_INTEGER, GL_INT, 0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
}

void ReadFeedbackBuffer(ecs_iter_t *it) {
    int f = 0;
    RingBuffer *feedback = ecs_field(it, RingBuffer, ++f);
    HoveredCell *hovered = ecs_field(it, HoveredCell, ++f);

    s32 pixels[4] = {0};
    glBindBuffer(GL_PIXEL_PACK_BUFFER, feedback->readableBuffer);
    glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, sizeof(pixels), &pixels);
    // NOTE: on some platforms may need to use glMapBufferRange instead. glGetBufferSubData is known to work on destop OpenGL and emscripten builds
    //void *buf = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, sizeof(pixels), GL_MAP_READ_BIT);
    // bc_gfxCheck();
    // if (buf != NULL) {
    //     memcpy(pixels, buf, sizeof(pixels));
    //     glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    // }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    hovered->x = pixels[0];
    hovered->y = pixels[1];
}

void BcviewSystemsTerrainImport(ecs_world_t *world) {
    ECS_MODULE(world, BcviewSystemsTerrain);

    // ECS_IMPORT(world, BcPlanes); // NOTE: don't worry about importing modules from the model, model components are guaranteed to be imported before any view systems (to keep component IDs identical between worlds)
    ECS_IMPORT(world, Bcview);
    ECS_IMPORT(world, BcviewPhases);

    // NOTE: won't run unless VideoSettings has a RenderDistance component before declaring this observer
    ECS_OBSERVER(world, InitTerrainBuffers, EcsOnAdd,
        [in] RenderDistance(VideoSettings),
        [out] TerrainBuffer,
        [out] ?QueryDesc,
        [none] bcview.TerrainRender,
    );

    ECS_OBSERVER(world, InitTerrainInstances, EcsOnAdd,
               [out] InstanceBuffer,
               [none] bcview.TerrainRender,
    );

    ECS_SYSTEM(world, UpdateTerrainInstances, OnModelChanged,
               [in] ModelQuery,
               [out] InstanceBuffer,
               [in] ModelWorld($),
               [none] bcview.TerrainRender,
    );

    ECS_SYSTEM(world, InitTerrainDataTexture, OnModelChanged,
               [in] ModelQuery,
               [out] !DataTexture,
               [in] ModelWorld($),
               [none] bcview.TerrainRender,
    );

    // ECS_OBSERVER(world, DestroyDataTexture, EcsOnDelete,
    //              DataTexture,
    //              ModelWorld($)
    // );

    ECS_SYSTEM(world, DestroyDataTextures, EcsOnLoad,
               [out] DataTexture,
               [none] !ModelWorld($)
    );

    // Only need to run when model step advances
    ECS_SYSTEM(world, UpdateTerrainDataTexture, OnModelChanged,
               [in] ModelQuery,
               [inout] DataTexture,
               [in] ModelWorld($),
               [none] bcview.TerrainRender,
    );

    // TODO: unused for now. Ideally can use UpdateTerrainDataTexture on step change, and this on single edits
    ECS_SYSTEM(world, UpdateTerrainDataTextureDirtyChunks, OnModelChanged,
               [in] ModelQuery,
               [inout] DataTexture,
               [inout] DirtyChunkBuffer($),
               [in] ModelWorld($),
               [none] bcview.TerrainRender,
    );

    ECS_SYSTEM(world, DrawHexTerrainShadows, OnPassShadow,
               [in] Pipeline(up(bcview.ShadowPass)),
               [in] InstanceBuffer,
               [in] Mesh,
               [in] DataTexture,
               [in] SunMatrix($),
               [none] ModelWorld($),
               [none] WrapInstanceOffsets($),
               [none] bcview.TerrainRender,
    );

    ECS_SYSTEM(world, DrawPipelineHexTerrain, OnPassGBuffer,
               [in] Pipeline(up(bcview.GBufferPass)),
               [in] InstanceBuffer,
               [in] Mesh,
               [in] DataTexture,
               [in] RingBuffer,
               [in] PVMatrix($),
               [in] Mouse($),
               [in] Clock($),
               [out] HoveredCell($),
               [none] ModelWorld($),
               [none] WrapInstanceOffsets($),
               [none] bcview.TerrainRender,
    );

    ECS_SYSTEM(world, ReadFeedbackBuffer, EcsOnStore,
               [in] RingBuffer,
               [out] HoveredCell($),
               [none] bcview.TerrainRender,
    );

    sg_reset_state_cache();

    // Pipeline, only need to create one per type
    ecs_entity_t terrainPipeline = ecs_set_name(world, 0, "Terrain Pipeline");
    ecs_add_pair(world, terrainPipeline, EcsChildOf, GBufferPass);
    ecs_set_pair(world, terrainPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/hexTerrain.vert"});
    // NOTE: as of 2023-7-29, there is an issue in Flecs where trigging an Observer by setitng a pair will cause the observer system to run BEFORE the component value is set. Workaround: trigger observer with a tag by adding it last, or set this up without observers
    ecs_set_pair(world, terrainPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/hexTerrain.frag"});
    ecs_set(world, terrainPipeline, PipelineDescription, {.shader_desc = &terrainShaderDescription, .pipeline_desc = &terrainPipelineDescription});

    ecs_entity_t terrainShadowPipeline = ecs_set_name(world, 0, "Terrain Shadow Pipeline");
    ecs_add_pair(world, terrainShadowPipeline, EcsChildOf, ShadowPass);
    ecs_set_pair(world, terrainShadowPipeline, ResourceFile, VertexShaderSource,   {.path = "view/shaders/shadow_terrain.vert"});
    ecs_set_pair(world, terrainShadowPipeline, ResourceFile, FragmentShaderSource, {.path = "view/shaders/shadow_terrain.frag"});
    ecs_set(world, terrainShadowPipeline, PipelineDescription, {.shader_desc = &terrainShadowShaderDescription, .pipeline_desc = &terrainShadowPipelineDescription});

    // TODO: give these setup steps a better home

    // init mesh
    //Mesh mesh = createHexmapMesh();
    Mesh mesh = createTriGridMesh(bc_chunkSize, bc_chunkSize, 1);

    // initialize RingBuffer entries
    RingBuffer pixelPackRingBuf = {0};
    for (int i = 0; i < RING_BUFFER_LENGTH; i++) {
        GLuint buf;
        glGenBuffers(1, &buf);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, buf);
        glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(int) * 4, NULL, GL_STREAM_READ);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        pixelPackRingBuf._buffers[i] = buf;
    }
    pixelPackRingBuf.readableBuffer = pixelPackRingBuf._buffers[0];
    pixelPackRingBuf.writableBuffer = pixelPackRingBuf._buffers[0];

    ecs_entity_t terrainDraw = ecs_set_name(world, 0, "TerrainDraw");
    ecs_add_pair(world, terrainDraw, GBufferPass, terrainPipeline);
    ecs_add_pair(world, terrainDraw, ShadowPass, terrainShadowPipeline);
    ecs_add(world, terrainDraw, TerrainRender);
    ecs_add(world, terrainDraw, TerrainBuffer);
    ecs_add(world, terrainDraw, InstanceBuffer);
    ecs_add(world, terrainDraw, QueryDesc);
    ecs_set_id(world, terrainDraw, ecs_id(Mesh), sizeof(mesh), &mesh);
    ecs_set_id(world, terrainDraw, ecs_id(RingBuffer), sizeof(pixelPackRingBuf), &pixelPackRingBuf);
}
