#ifndef KINGDOM_MAIN_H_INCLUDED
#define KINGDOM_MAIN_H_INCLUDED

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "ccVector.h"
#include "htw_core.h"
#include "htw_vulkan.h"
#include "kingdom_uiState.h"
#include "kingdom_worldState.h"

#define MAX_TEXT_LENGTH 256
#define TEXT_POOL_CAPACITY 8

// Radius of visible chunks around the camera center. 1 = only chunk containing camera is visible; 2 = 3x3 area; 3 = 5x5 area; etc.
#define MAX_VISIBLE_CHUNK_DISTANCE 3
#define VISIBLE_CHUNK_AREA_DIAMETER ((MAX_VISIBLE_CHUNK_DISTANCE * 2) - 1)
#define MAX_VISIBLE_CHUNKS (VISIBLE_CHUNK_AREA_DIAMETER * VISIBLE_CHUNK_AREA_DIAMETER)

// TODO: enums to describe the binding indicies for each descriptor set?

typedef struct {
    mat4x4 projection;
    mat4x4 view;
} kd_Camera;

typedef struct {
    htw_Buffer *buffers;
    u32 count;
    u32 maxCount;
} kd_BufferPool;

typedef struct {
    mat4x4 modelMatrix;
} kd_TextPoolItem;

typedef uint32_t kd_TextPoolItemHandle;

typedef struct {
    // texel position - pixel distance
    u32 offsetX;
    u32 offsetY;

    // glyph placement - pixel distance
    u32 width;
    u32 height;
    s32 bearingX;
    s32 bearingY;
    float advance;

    // texture uv - normalized position
    float u1;
    float v1;
    float u2;
    float v2;
} kd_GlyphMetrics;

/**
 * @brief state data needed to render text with a single font face.
 * Text is handled in pools: to render text, first request an item from the pool with aquireTextPoolItem()
 * Set the text you want to render by passing a pool item + your string to updateTextBuffer()
 * Update the position of rendered text by passing a pool item + transform matrix to setTextTransform()
 * To stop rendering text, pass a pool item to freeTextPoolItem() (internally this sets the scale to 0 and leaves the buffer unchanged)
 * After updating all the text you want displayed, calling drawText() will render everything in the text pool (although free pool items are effectively invisible)
 *
 */
typedef struct {
    FT_Library freetypeLibrary;
    FT_Face face;
    u32 pixelSize; // pixels per EM square; rough font size
    kd_GlyphMetrics *glyphMetrics;
    float unitsToPixels; // conversion factor from font units to pixels
    float lineDistance; // distance between baselines when adding a new line, in pixels
    float ascent; // when positioning by the top of a line, this is the distance to the baseline, in pixels
    htw_ModelData textModel;
    kd_TextPoolItem *textPool;
    htw_Buffer *uniformBuffer;
    htw_Buffer *bitmapBuffer;
    htw_Texture glyphBitmap;
    htw_DescriptorSetLayout textPipelineLayout;
    htw_DescriptorSet textPipelineDescriptorSet;
    htw_PipelineHandle textPipeline;
} kd_TextRenderContext;

typedef struct {
    vec3 position;
} kd_HexmapVertexData;

typedef struct {
    u32 chunkIndex;
    s32 bufferIndex;
} _htw_ChunkLoadTracker;

typedef struct {
    uint32_t subdivisions;
    htw_PipelineHandle pipeline;
    htw_DescriptorSetLayout chunkObjectLayout;
    htw_DescriptorSet *chunkObjectDescriptorSets;
    htw_ModelData modelData;
    // length MAX_VISIBLE_CHUNKS arrays that are compared to determine what chunks to reload each frame, and where to place them
    s32 *closestChunks;
    s32 *loadedChunks;
    // TODO: structure to keep track of both closest and currently loaded chunks, that can be easily compared every frame to determine what needs reloading
    size_t terrainChunkHostSize;
    size_t terrainChunkDeviceSize;
    htw_Buffer *terrainDataBuffer; // split into multiple logical buffers used to describe each terrain chunk
} kd_HexmapTerrain;

typedef struct {
    u32 width, height;
    u64 milliSeconds;
    u64 frame;
    // TODO: time of last frame or deltatime?
    htw_VkContext *vkContext;
    kd_BufferPool bufferPool;
    htw_Buffer *windowInfoBuffer;
    htw_Buffer *feedbackInfoBuffer; // Storage buffer written to by the fragment shader to find which cell the mouse is over
    htw_Buffer *worldInfoBuffer;
    htw_DescriptorSetLayout perFrameLayout;
    htw_DescriptorSet perFrameDescriptorSet;
    htw_DescriptorSetLayout perPassLayout;
    htw_DescriptorSet perPassDescriptorSet;
    kd_TextRenderContext textRenderContext;
    kd_HexmapTerrain surfaceTerrain;
    kd_HexmapTerrain caveTerrain;
    kd_Camera camera;
} kd_GraphicsState;

void kd_InitGraphics(kd_GraphicsState *graphics, u32 width, u32 height);
void kd_initWorldGraphics(kd_GraphicsState *graphics, kd_WorldState *world);
int kd_renderFrame(kd_GraphicsState *graphics, kd_UiState *ui, kd_WorldState *world);

#endif // KINGDOM_MAIN_H_INCLUDED
