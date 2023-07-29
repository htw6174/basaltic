
#include <ft2build.h>
#include FT_FREETYPE_H
#include "textRender.h"

// TEST
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define FT_CHECK(x) { FT_Error error = x; \
                    if(error) fprintf(stderr, "FreeType Error: %s failed with code %i\n", #x, x); }

typedef struct {
    float x;
    float y;
    float u;
    float v;
} TextBufferData;

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
    bc_GlyphMetrics *glyphMetrics;
    float unitsToPixels; // conversion factor from font units to pixels
    float lineDistance; // distance between baselines when adding a new line, in pixels
    float ascent; // when positioning by the top of a line, this is the distance to the baseline, in pixels
    bc_Model textModel;
    bc_TextPoolItem *textPool;
    // TODO: need to think about best way to pass window information and text instance data to shader (for 1: can use existing windowInfo buffer)
    htw_Buffer uniformBuffer;
    u8 *bitmap;
    size_t bitmapSize;
    htw_Buffer bitmapBuffer;
    htw_Texture glyphBitmap;
    htw_DescriptorSetLayout textPipelineLayout;
    htw_DescriptorSet textPipelineDescriptorSet;
    htw_PipelineHandle textPipeline;
} bc_TextRenderContext;

static void initTextGraphics(bc_GraphicsState *graphics) {
    static const u64 glyphCount = 256;
    bc_TextRenderContext *tc = &graphics->textRenderContext;
    // initialize pool
    tc->textPool = malloc(sizeof(bc_TextPoolItem) * TEXT_POOL_CAPACITY);
    for (int i = 0; i < TEXT_POOL_CAPACITY; i++) {
        mat4x4Zero(tc->textPool[i].modelMatrix);
    }

    tc->pixelSize = 48;
    FT_CHECK(FT_Init_FreeType(&tc->freetypeLibrary));
    FT_CHECK(FT_New_Face(tc->freetypeLibrary, "resources/fonts/NotoSansMono-Regular.ttf", 0, &tc->face));
    FT_CHECK(FT_Set_Pixel_Sizes(tc->face, 0, tc->pixelSize)); // NOTE: can also set size using dots+dpi
    tc->unitsToPixels = (float)tc->pixelSize / tc->face->units_per_EM;

    // load required glyphs
    tc->glyphMetrics = malloc(sizeof(bc_GlyphMetrics) * glyphCount);
    // get bitmap size info for assembling full bitmap later
    u32 totalWidth = 0;
    u32 maxHeight = 0;
    for (u64 c = 0; c < glyphCount; c++) {
        FT_CHECK(FT_Load_Char(tc->face, c, FT_LOAD_DEFAULT));
        totalWidth += tc->face->glyph->bitmap.width;
        maxHeight = max_int(maxHeight, tc->face->glyph->bitmap.rows);
    }
    tc->lineDistance = (tc->face->ascender - tc->face->descender) * tc->unitsToPixels; // technically should add line_gap here, but it is part of the truetype specific interface and this is good enough for now
    tc->ascent = tc->face->ascender * tc->unitsToPixels;

    static const int bytesPerTexel = sizeof(char);
    u32 bitmapWidth = htw_nextPow(totalWidth);
    u32 bitmapHeight = htw_nextPow(maxHeight);
    tc->bitmapSize = bytesPerTexel * bitmapWidth * bitmapHeight;
    tc->bitmap = malloc(tc->bitmapSize);

    // create resource buffers
    tc->uniformBuffer = htw_createBuffer(graphics->vkContext, graphics->bufferPool, 256, HTW_BUFFER_USAGE_UNIFORM); // TODO: determine full size
    tc->bitmapBuffer = htw_createBuffer(graphics->vkContext, graphics->bufferPool, tc->bitmapSize, HTW_BUFFER_USAGE_TEXTURE);

    // assemble full ASCII font bitmap
    // TODO: eliminate duplicate 'missing' characters, reuse glyph metrics for special characters
    int bitmapX = 0;
    for (int i = 0; i < glyphCount; i++) {
        FT_CHECK(FT_Load_Char(tc->face, i, FT_LOAD_RENDER));
        // save glyph metrics
        FT_GlyphSlot glyph = tc->face->glyph;
        bc_GlyphMetrics gm = {
            .offsetX = bitmapX,
            .offsetY = 0, // change if using a 2d glyph layout
            .width = glyph->bitmap.width,
            .height = glyph->bitmap.rows,
            .bearingX = glyph->bitmap_left,
            .bearingY = glyph->bitmap_top,
            .advance = (float)glyph->advance.x / 64.0, // glyph gives this in dots, we want it in pixels
            .u1 = ((float)bitmapX + 0.5) / bitmapWidth, // adding 0.5 to the base bitmap coord here fixes an alignment issue with glyph uvs. Without this correction, you can see one line of the previous glyph on the left side of every character
            .v1 = 0.0,
            .u2 = gm.u1 + ((float)glyph->bitmap.width / bitmapWidth),
            .v2 = gm.v1 + ((float)glyph->bitmap.rows / bitmapHeight)
        };
        tc->glyphMetrics[i] = gm;
        //printf("width, height for %c: %i, %i\n", (char)i, gm.width, gm.height);

        // insert glyph into bitmap
        unsigned char *buffer = glyph->bitmap.buffer;
        for (int row = 0; row < gm.height; row++) {
            unsigned char *dest = tc->bitmap + bitmapX + (row * bitmapWidth);
            unsigned char *src = buffer + (row * gm.width);
            memcpy(dest, src, gm.width);
        }
        bitmapX += gm.width;
    }

    // TEST: write bitmap
    //int result = stbi_write_bmp("/home/htw/projects/c/basaltic/output/font.bmp", bitmapWidth, bitmapHeight, 1, bitmap);
    //printf("bitmap write returned %i\n", result);

    // create texture
    tc->glyphBitmap = htw_createGlyphTexture(graphics->vkContext, bitmapWidth, bitmapHeight);

    // create mesh buffers
    u32 vertsPerString = MAX_TEXT_LENGTH * 4;
    u32 vertexCount = vertsPerString * TEXT_POOL_CAPACITY;
    size_t vertexDataSize = sizeof(TextBufferData) * vertexCount;
    tc->textModel.vertexDataSize = vertexDataSize;
    tc->textModel.vertexData = malloc(vertexDataSize);
    tc->textModel.model.vertexCount = vertexCount;
    tc->textModel.model.vertexBuffer = htw_createBuffer(graphics->vkContext, graphics->bufferPool, vertexDataSize, HTW_BUFFER_USAGE_VERTEX);
    u32 indexCount = MAX_TEXT_LENGTH * 6;
    size_t indexDataSize = sizeof(u32) * indexCount;
    tc->textModel.indexDataSize = indexDataSize;
    tc->textModel.indexData = malloc(indexDataSize);
    tc->textModel.model.indexCount = indexCount;
    tc->textModel.model.indexBuffer = htw_createBuffer(graphics->vkContext, graphics->bufferPool, indexDataSize, HTW_BUFFER_USAGE_INDEX);
    tc->textModel.model.instanceCount = 0;

    // fill index buffer (same for all buffers in a pool)
    u32 *indexData = (u32*)tc->textModel.indexData;
    u32 tIndex = 0;
    for (int v = 0; v < vertsPerString; v += 4) {
        indexData[tIndex++] = v + 0;
        indexData[tIndex++] = v + 1;
        indexData[tIndex++] = v + 2;
        indexData[tIndex++] = v + 1;
        indexData[tIndex++] = v + 3;
        indexData[tIndex++] = v + 2;
    }

    // setup shaders
    htw_ShaderInputInfo vertInfo = {
        .size = sizeof(TextBufferData),
        .offset = 0,
        .inputType = HTW_VERTEX_TYPE_FLOAT
    };
    htw_ShaderSet shaderInfo = {
        .vertexShader = htw_loadShader(graphics->vkContext, "shaders_bin/text.vert.spv"),
        .fragmentShader = htw_loadShader(graphics->vkContext, "shaders_bin/text.frag.spv"),
        .vertexInputStride = sizeof(TextBufferData),
        .vertexInputCount = 1,
        .vertexInputInfos = &vertInfo,
        .instanceInputCount = 0
    };

    // create pipeline
    tc->textPipelineLayout = htw_createTextPipelineSetLayout(graphics->vkContext);
    tc->textPipelineDescriptorSet = htw_allocateDescriptor(graphics->vkContext, tc->textPipelineLayout);
    htw_DescriptorSetLayout textPipelineLayouts[] = {graphics->perFrameLayout, graphics->perPassLayout, tc->textPipelineLayout, NULL};
    tc->textPipeline = htw_createPipeline(graphics->vkContext, textPipelineLayouts, shaderInfo);
}

static void writeTextBuffers(bc_GraphicsState *graphics) {
    bc_TextRenderContext *tc = &graphics->textRenderContext;

    htw_writeBuffer(graphics->vkContext, tc->bitmapBuffer, tc->bitmap, tc->bitmapSize);
    htw_updateTexture(graphics->vkContext, tc->bitmapBuffer, tc->glyphBitmap);

    htw_writeBuffer(graphics->vkContext, tc->textModel.model.indexBuffer, tc->textModel.indexData, tc->textModel.indexDataSize);
}

static void updateTextDescriptors(bc_GraphicsState *graphics) {
    bc_TextRenderContext *tc = &graphics->textRenderContext;

    htw_updateTextDescriptor(graphics->vkContext, tc->textPipelineDescriptorSet, tc->uniformBuffer, tc->glyphBitmap);
}

static bc_TextPoolItemHandle aquireTextPoolItem(bc_TextRenderContext *tc) {
    // TODO: determine next free pool item
    return 0;
}

static void updateTextBuffer(bc_GraphicsState *graphics, bc_TextPoolItemHandle poolItem, char *text) {
    bc_TextRenderContext *tc = &graphics->textRenderContext;

    // fill buffer
    size_t bufferOffset = poolItem * MAX_TEXT_LENGTH;
    TextBufferData *vertexBuffer = (TextBufferData*)tc->textModel.vertexData + bufferOffset;
    int vIndex = 0;
    float xOrigin = 0.0;
    float yOrigin = tc->ascent;
    for (int i = 0; i < MAX_TEXT_LENGTH; i++) {
        unsigned char c = text[i];
        if (c == '\0') break; // FIXME: change behavior here - go through the entire available space, but set everything to 0 after end of string
        if (c == '\n') {
            // CR, LF
            xOrigin = 0.0;
            yOrigin += tc->lineDistance;
        } else {
            bc_GlyphMetrics gm = tc->glyphMetrics[c];
            float top = yOrigin - gm.bearingY;
            float bottom = top + gm.height;
            float left = gm.bearingX + xOrigin;
            float right = left + gm.width;

            // fill vertex buffer
            {
            TextBufferData v1 = {
                .x = left,
                .y = top,
                .u = gm.u1,
                .v = gm.v1
            };
            vertexBuffer[vIndex++] = v1;

            TextBufferData v2 = {
                .x = right,
                .y = top,
                .u = gm.u2,
                .v = gm.v1
            };
            vertexBuffer[vIndex++] = v2;

            TextBufferData v3 = {
                .x = left,
                .y = bottom,
                .u = gm.u1,
                .v = gm.v2
            };
            vertexBuffer[vIndex++] = v3;

            TextBufferData v4 = {
                .x = right,
                .y = bottom,
                .u = gm.u2,
                .v = gm.v2
            };
            vertexBuffer[vIndex++] = v4;
            }

            xOrigin += gm.advance;
        }
    }

    htw_writeBuffer(graphics->vkContext, tc->textModel.model.vertexBuffer, tc->textModel.vertexData, tc->textModel.vertexDataSize); // TODO: update only the subset of the buffer that changed
}

static void setTextTransform(bc_TextRenderContext *tc, bc_TextPoolItemHandle poolItem, mat4x4 modelMatrix) {
    // TODO
    memcpy(tc->textPool[poolItem].modelMatrix, modelMatrix, sizeof(mat4x4));
}

static void freeTextPoolItem(bc_TextRenderContext *tc, bc_TextPoolItemHandle poolItem) {
    // TODO
}

static void drawText(bc_GraphicsState *graphics) {
    bc_TextRenderContext *tc = &graphics->textRenderContext;

    // TEST
    htw_writeBuffer(graphics->vkContext, tc->uniformBuffer, tc->textPool[0].modelMatrix, sizeof(mat4x4));

    htw_bindPipeline(graphics->vkContext, tc->textPipeline);
    htw_bindDescriptorSet(graphics->vkContext, tc->textPipeline, tc->textPipelineDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_PIPELINE);
    htw_drawPipeline(graphics->vkContext, tc->textPipeline, &tc->textModel.model, HTW_DRAW_TYPE_INDEXED);
}

static void drawDebugText(bc_GraphicsState *graphics, bc_UiState *ui) {
    bc_TextPoolItemHandle textItem = aquireTextPoolItem(&graphics->textRenderContext);
    char statusText[256];
    // sprintf(statusText,
    //         "Mouse Position: %4.4i, %4.4i\nChunk Index: %4.4u\nHighlighted Cell: %4.4u",
    //         ui->mouse.x, ui->mouse.y, ui->hoveredChunkIndex, ui->hoveredCellIndex);
    sprintf(statusText, "Press backquote (`/~) to toggle editor, \nthen press 'Start new game'");
    updateTextBuffer(graphics, textItem, statusText);
    mat4x4 textTransform;
    // NOTE: ccVector's matnxnSet* methods start from a new identity matrix and overwrite the *entire* input matrix
    // NOTE: remember that the order of translate/rotate/scale matters (you probably want to scale first)
    mat4x4SetScale(textTransform, 1.0); // TODO: figure out how to resolve bleed from neighboring glyphs when scaling text mesh
    //mat4x4Translate(textTransform, (vec3){{-1.0, -1.0, 0.0}}); // Position in top-left corner
    mat4x4Translate(textTransform, (vec3){{-0.5, 0.0, 0.0}}); // Position roughly in middle
    setTextTransform(&graphics->textRenderContext, textItem, textTransform);
    drawText(graphics);

//     drawText(graphics, "1234567890_abcdefghijklmnopqrstuvwxyz", (vec3){{-600.f, -40.f, 0.f}});
//     char allChars[128];
//     for (int i = 0; i < 128; i++) {
//         allChars[i] = (char)i;
//     }
//     allChars[0] = '0';
//     drawText(graphics, allChars, (vec3){{-1280.f, 0.f, 0.f}});
}
