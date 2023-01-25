#ifndef TEXTRENDER_H_INCLUDED
#define TEXTRENDER_H_INCLUDED

#include "htw_core.h"
#include "ccVector.h"

#define MAX_TEXT_LENGTH 256
#define TEXT_POOL_CAPACITY 8

typedef struct {
    mat4x4 modelMatrix;
} bc_TextPoolItem;

typedef uint32_t bc_TextPoolItemHandle;

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
} bc_GlyphMetrics;

static void initTextGraphics(bc_GraphicsState *graphics);
static void writeTextBuffers(bc_GraphicsState *graphics);
static void updateTextDescriptors(bc_GraphicsState *graphics);
static bc_TextPoolItemHandle aquireTextPoolItem(bc_TextRenderContext *tc);
static void updateTextBuffer(bc_GraphicsState *graphics, bc_TextPoolItemHandle poolItem, char *text);
static void setTextTransform(bc_TextRenderContext *tc, bc_TextPoolItemHandle poolItem, mat4x4 modelMatrix);
static void freeTextPoolItem(bc_TextRenderContext *tc, bc_TextPoolItemHandle poolItem); // TODO: consider returning something to indicate result (success, item doesn't exist, item already free)
static void drawText(bc_GraphicsState *graphics);
static void drawDebugText(bc_GraphicsState *graphics, bc_UiState *ui);

#endif // TEXTRENDER_H_INCLUDED
