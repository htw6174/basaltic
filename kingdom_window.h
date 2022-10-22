#ifndef KINGDOM_MAIN_H_INCLUDED
#define KINGDOM_MAIN_H_INCLUDED

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "ccVector.h"
#include "htw_vulkan.h"
#include "kingdom_uiState.h"
#include "kingdom_worldState.h"

#define PI 3.141592f
#define DEG_TO_RAD 0.017453f

#define MAX_TEXT_LENGTH 256
#define TEXT_POOL_CAPACITY 256

typedef struct {
    mat4x4 projection;
    mat4x4 view;
} kd_Camera;

typedef struct kd_BufferPool {
    htw_Buffer *buffers;
    unsigned int count;
    unsigned int maxCount;
} kd_BufferPool;

typedef struct kd_TextPoolItem {
    mat4x4 modelMatrix;
} kd_TextPoolItem;

typedef uint32_t kd_TextPoolItemHandle;

typedef struct kd_GlyphMetrics {
    // texel position - pixel distance
    unsigned int offsetX;
    unsigned int offsetY;

    // glyph placement - pixel distance
    unsigned int width;
    unsigned int height;
    int bearingX;
    int bearingY;
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
typedef struct kd_TextRenderContext {
    FT_Library freetypeLibrary;
    FT_Face face;
    unsigned int pixelSize; // pixels per EM square; rough font size
    kd_GlyphMetrics *glyphMetrics;
    float unitsToPixels; // conversion factor from font units to pixels
    float lineDistance; // distance between baselines when adding a new line, in pixels
    float ascent; // when positioning by the top of a line, this is the distance to the baseline, in pixels
    htw_ModelData textModel;
    kd_TextPoolItem *textPool;
    htw_Buffer *uniformBuffer;
    htw_Buffer *bitmapBuffer;
    htw_Texture glyphBitmap;
    htw_ShaderLayout shaderLayout;
    htw_PipelineHandle textPipeline;
} kd_TextRenderContext;

typedef struct kd_HexmapTerrain {
    htw_ValueMap *heightMap;
    uint32_t subdivisions;
    htw_PipelineHandle pipeline;
    htw_ShaderLayout shaderLayout;
    htw_ModelData modelData;
    htw_Buffer *worldInfoBuffer;
    htw_Buffer *terrainDataBuffer;
    //uint32_t *tileVertexIndicies;
} kd_HexmapTerrain;

typedef struct kd_InstanceTerrain {
    htw_ValueMap *heightMap;
    htw_PipelineHandle pipeline;
    htw_ModelData modelData;
} kd_InstanceTerrain;

typedef struct kd_HexmapVertexData {
    vec3 position;
} kd_HexmapVertexData;

typedef struct kd_GraphicsState {
    unsigned int width, height;
    uint64_t milliSeconds;
    uint64_t frame;
    // TODO: time of last frame or deltatime?
    htw_VkContext *vkContext;
    kd_BufferPool bufferPool;
    kd_TextRenderContext textRenderContext;
    kd_HexmapTerrain surfaceTerrain;
    kd_HexmapTerrain caveTerrain;
    kd_InstanceTerrain instanceTerrain;
    kd_Camera camera;
} kd_GraphicsState;

void kd_InitGraphics(kd_GraphicsState *graphics, unsigned int width, unsigned int height);
void kd_initWorldGraphics(kd_GraphicsState *graphics, kd_WorldState *world);
int kd_renderFrame(kd_GraphicsState *graphics, kd_UiState *ui, kd_WorldState *world);

#endif // KINGDOM_MAIN_H_INCLUDED
