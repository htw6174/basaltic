#ifndef HEXMAPRENDER_H_INCLUDED
#define HEXMAPRENDER_H_INCLUDED

#include "htw_core.h"
#include "htw_vulkan.h"
#include "htw_geomap.h"
#include "ccVector.h"
#include "basaltic_mesh.h"
#include "basaltic_worldState.h"


typedef struct {
    vec3 position;
    u32 cellIndex;
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

// NOTE: this struct isn't meant to be used directly - it exists to find the offset of an array of _bc_TerrainCellData packed into a buffer after the other fields in this struct
typedef struct {
    u32 chunkIndex;
    bc_CellData chunkData;
} bc_TerrainBufferData;

// Unchanging model data needed to render any hexmap terrain chunk; only need one instance
typedef struct {
    uint32_t subdivisions; // TODO: unused
    htw_PipelineHandle pipeline;
    htw_DescriptorSetLayout chunkObjectLayout;
    bc_Mesh chunkMesh;
} bc_RenderableHexmap;

// Buffer data for an entire terrain layer, and tracking for what chunk data needs to be loaded each frame
typedef struct {
    // Radius of visible chunks around the camera center. 1 = only chunk containing camera is visible; 2 = 3x3 area; 3 = 5x5 area; etc.
    u32 renderedChunkRadius;
    u32 renderedChunkCount;
    // length renderedChunkCount arrays that are compared to determine what chunks to reload each frame, and where to place them
    s32 *closestChunks;
    s32 *loadedChunks;
    size_t terrainBufferSize;
    bc_TerrainBufferData *terrainBufferData;
    htw_DescriptorSet *chunkObjectDescriptorSets;
    htw_SplitBuffer terrainBuffer; // split into multiple logical buffers used to describe each terrain chunk
} bc_HexmapTerrain;

bc_RenderableHexmap *bc_createRenderableHexmap(htw_VkContext *vkContext, htw_BufferPool bufferPool, htw_DescriptorSetLayout perFrameLayout, htw_DescriptorSetLayout perPassLayout);
bc_HexmapTerrain *bc_createHexmapTerrain(htw_VkContext *vkContext, htw_BufferPool bufferPool, bc_RenderableHexmap *hexmap, u32 visibilityRadius);

// TODO: do these need to be seperate methods from other setup?
void bc_writeTerrainBuffers(htw_VkContext *vkContext, bc_RenderableHexmap *hexmap);
void bc_updateHexmapDescriptors(htw_VkContext *vkContext, bc_HexmapTerrain *terrain);

void bc_updateTerrainVisibleChunks(htw_VkContext *vkContext, htw_ChunkMap *chunkMap, bc_HexmapTerrain *terrain, u32 centerChunk);
void bc_drawHexmapTerrain(htw_VkContext *vkContext, htw_ChunkMap *chunkMap, bc_RenderableHexmap *hexmap, bc_HexmapTerrain *terrain);

#endif // HEXMAPRENDER_H_INCLUDED
