#ifndef KINGDOM_MAIN_H_INCLUDED
#define KINGDOM_MAIN_H_INCLUDED

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "ccVector.h"
#include "htw_core.h"
#include "htw_vulkan.h"
#include "basaltic_uiState.h"
#include "basaltic_worldState.h"

#define MAX_TEXT_LENGTH 256
#define TEXT_POOL_CAPACITY 8

// Radius of visible chunks around the camera center. 1 = only chunk containing camera is visible; 2 = 3x3 area; 3 = 5x5 area; etc.
#define MAX_VISIBLE_CHUNK_DISTANCE 2
#define VISIBLE_CHUNK_AREA_DIAMETER ((MAX_VISIBLE_CHUNK_DISTANCE * 2) - 1)
#define MAX_VISIBLE_CHUNKS (VISIBLE_CHUNK_AREA_DIAMETER * VISIBLE_CHUNK_AREA_DIAMETER)

typedef struct {
    mat4x4 projection;
    mat4x4 view;
} bt_Camera;

typedef struct {
    vec2 windowSize;
    vec2 mousePosition;
    // only xyz used, but needs to be vec4 for shader uniform buffer alignment
    vec4 cameraPosition;
    vec4 cameraFocalPoint;
} _bt_WindowInfo;

typedef struct {
    u32 chunkIndex;
    u32 cellIndex;
} _bt_FeedbackInfo;

typedef struct {
    vec3 gridToWorld;
    float totalWidth;
    s32 timeOfDay;
    u32 visibilityOverrideBits;
} _bt_WorldInfo;

typedef struct {
    htw_ModelData model;
    void *vertexData;
    void *indexData;
    void *instanceData;
    size_t vertexDataSize;
    size_t indexDataSize;
    size_t instanceDataSize;
} bt_Model;

typedef struct {
    mat4x4 modelMatrix;
} bt_TextPoolItem;

typedef uint32_t bt_TextPoolItemHandle;

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
} bt_GlyphMetrics;

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
    bt_GlyphMetrics *glyphMetrics;
    float unitsToPixels; // conversion factor from font units to pixels
    float lineDistance; // distance between baselines when adding a new line, in pixels
    float ascent; // when positioning by the top of a line, this is the distance to the baseline, in pixels
    bt_Model textModel;
    bt_TextPoolItem *textPool;
    // TODO: need to think about best way to pass window information and text instance data to shader (for 1: can use existing windowInfo buffer)
    htw_Buffer uniformBuffer;
    u8 *bitmap;
    size_t bitmapSize;
    htw_Buffer bitmapBuffer;
    htw_Texture glyphBitmap;
    htw_DescriptorSetLayout textPipelineLayout;
    htw_DescriptorSet textPipelineDescriptorSet;
    htw_PipelineHandle textPipeline;
} bt_TextRenderContext;

typedef struct {
    vec3 position;
    u32 cellIndex;
} bt_HexmapVertexData;

typedef struct {
    s16 elevation;
    u8 paletteX;
    u8 paletteY;
    u8 visibilityBits;
    u8 lightingBits; // TODO: use one of these bits for solid underground areas? Could override elevation as well to create walls
    u16 unused2; // weather / temporary effect bitmask?
    //int64_t aligner;
} _bt_TerrainCellData; // TODO: move this to a world logic source file? keep the data in a format that's useful for rendering (will be useful for terrain lookup and updates too)

// NOTE: this struct isn't meant to be used directly - it exists to find the offset of an array of _bt_TerrainCellData packed into a buffer after the other fields in this struct
typedef struct {
    u32 chunkIndex;
    _bt_TerrainCellData chunkData;
} _bt_TerrainBufferData;

typedef struct {
    // TODO: everything other than terrain buffer data can be used for multiple different terrains (surface and cave layers). Split this struct to reflect that, only keep one copy of the base terrain model
    uint32_t subdivisions;
    htw_PipelineHandle pipeline;
    htw_DescriptorSetLayout chunkObjectLayout;
    htw_DescriptorSet *chunkObjectDescriptorSets;
    bt_Model chunkModel;
    // length MAX_VISIBLE_CHUNKS arrays that are compared to determine what chunks to reload each frame, and where to place them
    s32 *closestChunks;
    s32 *loadedChunks;
    size_t terrainChunkSize;
    _bt_TerrainBufferData *terrainBufferData;
    htw_SplitBuffer terrainBuffer; // split into multiple logical buffers used to describe each terrain chunk
} bt_HexmapTerrain;

typedef struct {
    u32 maxInstanceCount;
    htw_PipelineHandle pipeline;
    htw_DescriptorSetLayout debugLayout;
    htw_DescriptorSet debugDescriptorSet;
    bt_Model instancedModel;
} bt_DebugSwarm;

typedef struct {
    u32 width, height;
    u64 milliSeconds;
    u64 frame;
    // TODO: time of last frame or deltatime?
    htw_VkContext *vkContext;
    htw_BufferPool bufferPool;

    _bt_WindowInfo windowInfo;
    _bt_FeedbackInfo feedbackInfo;
    _bt_WorldInfo worldInfo;
    htw_Buffer windowInfoBuffer;
    htw_Buffer feedbackInfoBuffer; // Storage buffer written to by the fragment shader to find which cell the mouse is over
    htw_Buffer worldInfoBuffer;

    htw_DescriptorSetLayout perFrameLayout;
    htw_DescriptorSet perFrameDescriptorSet;
    htw_DescriptorSetLayout perPassLayout;
    htw_DescriptorSet perPassDescriptorSet;
    bt_TextRenderContext textRenderContext;
    bt_HexmapTerrain surfaceTerrain;
    bt_HexmapTerrain caveTerrain;
    bool showCharacterDebug;
    bt_DebugSwarm characterDebug;
    bt_Camera camera;
} bt_GraphicsState;

void bt_initGraphics(bt_GraphicsState *graphics, u32 width, u32 height);
void bt_initWorldGraphics(bt_GraphicsState *graphics, bt_WorldState *world);
int bt_drawFrame(bt_GraphicsState *graphics, bt_UiState *ui, bt_WorldState *world);
void bt_endFrame(bt_GraphicsState *graphics);
void bt_destroyGraphics(bt_GraphicsState *graphics);

#endif // KINGDOM_MAIN_H_INCLUDED
