#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "ccVector.h"
#include "htw_core.h"
#include "htw_random.h"
#include "htw_vulkan.h"
#include "kingdom_window.h"
#include "kingdom_logicInputState.h"
#include "kingdom_worldState.h"

// TEST
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define FT_CHECK(x) { FT_Error error = x; \
                    if(error) fprintf(stderr, "FreeType Error: %s failed with code %i\n", #x, x); }

typedef struct {
    mat4x4 pv;
    mat4x4 m;
} PushConstantData;

typedef struct {
    vec2 windowSize;
    vec2 mousePosition;
    // only xyz used, but needs to be vec4 for shader uniform buffer alignment
    vec4 cameraPosition;
    vec4 cameraFocalPoint;
} WindowInfo;

typedef struct {
    u32 chunkIndex;
    u32 cellIndex;
} FeedbackInfo;

typedef struct {
    int timeOfDay;
} WorldInfo;

typedef struct {
    float x;
    float y;
    float u;
    float v;
} TextBufferData;

typedef struct {
    s16 elevation;
    //u16 palleteIndex;
    u8 paletteX;
    u8 paletteY;
    s16 unused1; // weather / temporary effect bitmask?
    s16 unused2; // lighting / player visibility bitmask?
    //int64_t aligner;
} TerrainBufferData; // TODO: move this to a world logic source file? keep the data in a format that's useful for rendering (will be useful for terrain lookup and updates too)

static htw_VkContext *createWindow(u32 width, u32 height);
static void mapCamera(kd_GraphicsState *graphics, htw_PipelineHandle pipeline);

static kd_BufferPool createBufferPool(u32 maxCount); // TODO: if buffer collection management is done by htw_vulkan, is this still needed?
static htw_Buffer* getNextBuffer(kd_BufferPool *pool);

static void updateWindowInfoBuffer(kd_GraphicsState *graphics, s32 mouseX, s32 mouseY, vec4 cameraPosition, vec4 cameraFocalPoint);

static void initTextGraphics(kd_GraphicsState *graphics);
static void updateTextDescriptors(kd_GraphicsState *graphics);
static kd_TextPoolItemHandle aquireTextPoolItem(kd_TextRenderContext *tc);
static void updateTextBuffer(kd_GraphicsState *graphics, kd_TextPoolItemHandle poolItem, char *text);
static void setTextTransform(kd_TextRenderContext *tc, kd_TextPoolItemHandle poolItem, mat4x4 modelMatrix);
static void freeTextPoolItem(kd_TextRenderContext *tc, kd_TextPoolItemHandle poolItem); // TODO: consider returning something to indicate result (success, item doesn't exist, item already free)
static void drawText(kd_GraphicsState *graphics);

static void initTerrainGraphics(kd_GraphicsState *graphics, kd_WorldState *world);

static void createHexmapMesh(kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain);
//static void createHexmapTerrain(kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain);
static void updateHexmapDescriptors(kd_GraphicsState *graphics, kd_HexmapTerrain *terrain);
static void updateHexmapVisibleChunks(kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain, u32 centerChunk);
static void updateHexmapDataBuffer (kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain, u32 chunkIndex, u32 subBufferIndex);
static void setHexmapBufferFromCell(kd_MapChunk *chunk, u32 cellX, u32 cellY, TerrainBufferData *target);

static vec3 getRandomColor();

void kd_InitGraphics(kd_GraphicsState *graphics, u32 width, u32 height) {
    graphics->width = width;
    graphics->height = height;
    graphics->vkContext = createWindow(width, height);
    graphics->bufferPool = createBufferPool(100);
    graphics->frame = 0;

    graphics->windowInfoBuffer = getNextBuffer(&graphics->bufferPool);
    *graphics->windowInfoBuffer = htw_createBuffer(graphics->vkContext, sizeof(WindowInfo), HTW_BUFFER_USAGE_UNIFORM);

    graphics->perFrameLayout = htw_createPerFrameSetLayout(graphics->vkContext);
    graphics->perPassLayout = htw_createPerPassSetLayout(graphics->vkContext);
    graphics->perFrameDescriptorSet = htw_allocateDescriptor(graphics->vkContext, graphics->perFrameLayout);
    graphics->perPassDescriptorSet = htw_allocateDescriptor(graphics->vkContext, graphics->perPassLayout);

    // create per frame descriptor buffers
    graphics->worldInfoBuffer = getNextBuffer(&graphics->bufferPool);
    graphics->feedbackInfoBuffer = getNextBuffer(&graphics->bufferPool);
    *graphics->worldInfoBuffer = htw_createBuffer(graphics->vkContext, sizeof(WorldInfo), HTW_BUFFER_USAGE_UNIFORM);
    *graphics->feedbackInfoBuffer = htw_createBuffer(graphics->vkContext, sizeof(FeedbackInfo), HTW_BUFFER_USAGE_STORAGE);

    // TODO: write world info
    WorldInfo *worldInfo = graphics->worldInfoBuffer->hostData;

    // initialize highlighted cell
    FeedbackInfo *feedbackInfo = graphics->feedbackInfoBuffer->hostData;
    feedbackInfo->cellIndex = 0;
    feedbackInfo->chunkIndex = 0;
}

htw_VkContext *createWindow(u32 width, u32 height) {
    SDL_Window *sdlWindow = SDL_CreateWindow("kingdom", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, HTW_VK_WINDOW_FLAGS);
    htw_VkContext *context = htw_createVkContext(sdlWindow);
    return context;
}

void kd_initWorldGraphics(kd_GraphicsState *graphics, kd_WorldState *world) {
    initTextGraphics(graphics);
    initTerrainGraphics(graphics, world);

    htw_finalizeBuffers(graphics->vkContext, graphics->bufferPool.count, graphics->bufferPool.buffers);
    htw_bindBuffers(graphics->vkContext, graphics->bufferPool.count, graphics->bufferPool.buffers);
    htw_updateBuffers(graphics->vkContext, graphics->bufferPool.count, graphics->bufferPool.buffers);

    htw_beginOneTimeCommands(graphics->vkContext);
    htw_updatePerFrameDescriptor(graphics->vkContext, graphics->perFrameDescriptorSet, *graphics->windowInfoBuffer, *graphics->feedbackInfoBuffer, *graphics->worldInfoBuffer);
    updateTextDescriptors(graphics);
    updateHexmapDescriptors(graphics, &graphics->surfaceTerrain);
    htw_endOneTimeCommands(graphics->vkContext);

}

static void updateWindowInfoBuffer(kd_GraphicsState *graphics, s32 mouseX, s32 mouseY, vec4 cameraPosition, vec4 cameraFocalPoint) {
    WindowInfo *wdi = (WindowInfo *)graphics->windowInfoBuffer->hostData;
    wdi->windowSize = (vec2){{graphics->width, graphics->height}};
    wdi->mousePosition = (vec2){{mouseX, mouseY}};
    wdi->cameraPosition = cameraPosition;
    wdi->cameraFocalPoint = cameraFocalPoint;
    htw_updateBuffer(graphics->vkContext, graphics->windowInfoBuffer);
}

static void initTextGraphics(kd_GraphicsState *graphics) {
    static const u64 glyphCount = 256;
    kd_TextRenderContext *tc = &graphics->textRenderContext;
    // initialize pool
    tc->textPool = malloc(sizeof(kd_TextPoolItem) * TEXT_POOL_CAPACITY);
    for (int i = 0; i < TEXT_POOL_CAPACITY; i++) {
        mat4x4Zero(tc->textPool[i].modelMatrix);
    }

    tc->pixelSize = 48;
    FT_CHECK(FT_Init_FreeType(&tc->freetypeLibrary));
    FT_CHECK(FT_New_Face(tc->freetypeLibrary, "resources/fonts/NotoSansMono-Regular.ttf", 0, &tc->face));
    FT_CHECK(FT_Set_Pixel_Sizes(tc->face, 0, tc->pixelSize)); // NOTE: can also set size using dots+dpi
    tc->unitsToPixels = (float)tc->pixelSize / tc->face->units_per_EM;

    // load required glyphs
    tc->glyphMetrics = malloc(sizeof(kd_GlyphMetrics) * glyphCount);
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
    size_t bitmapSize = bytesPerTexel * bitmapWidth * bitmapHeight;

    // create resource buffers
    tc->uniformBuffer = getNextBuffer(&graphics->bufferPool);
    tc->bitmapBuffer = getNextBuffer(&graphics->bufferPool);
    *tc->uniformBuffer = htw_createBuffer(graphics->vkContext, 256, HTW_BUFFER_USAGE_UNIFORM); // TODO: determine full size
    *tc->bitmapBuffer = htw_createBuffer(graphics->vkContext, bitmapSize, HTW_BUFFER_USAGE_TEXTURE);

    // assemble full ASCII font bitmap
    // TODO: eliminate duplicate 'missing' characters, reuse glyph metrics for special characters
    unsigned char *bitmap = tc->bitmapBuffer->hostData;
    int bitmapX = 0;
    for (int i = 0; i < glyphCount; i++) {
        FT_CHECK(FT_Load_Char(tc->face, i, FT_LOAD_RENDER));
        // save glyph metrics
        FT_GlyphSlot glyph = tc->face->glyph;
        kd_GlyphMetrics gm = {
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
            unsigned char *dest = bitmap + bitmapX + (row * bitmapWidth);
            unsigned char *src = buffer + (row * gm.width);
            memcpy(dest, src, gm.width);
        }
        bitmapX += gm.width;
    }

    // TEST: write bitmap
    //int result = stbi_write_bmp("/home/htw/projects/c/kingdom/output/font.bmp", bitmapWidth, bitmapHeight, 1, bitmap);
    //printf("bitmap write returned %i\n", result);

    // create texture
    tc->glyphBitmap = htw_createGlyphTexture(graphics->vkContext, bitmapWidth, bitmapHeight);

    // create mesh buffers
    tc->textModel.vertexBuffer = getNextBuffer(&graphics->bufferPool);
    tc->textModel.indexBuffer = getNextBuffer(&graphics->bufferPool);
    tc->textModel.instanceCount = 0;
    u32 vertsPerString = MAX_TEXT_LENGTH * 4;
    u32 vertexCount = vertsPerString * TEXT_POOL_CAPACITY;
    tc->textModel.vertexCount = vertexCount;
    *tc->textModel.vertexBuffer = htw_createBuffer(graphics->vkContext, sizeof(TextBufferData) * vertexCount, HTW_BUFFER_USAGE_VERTEX);
    u32 indexCount = MAX_TEXT_LENGTH * 6;
    tc->textModel.indexCount = indexCount;
    *tc->textModel.indexBuffer = htw_createBuffer(graphics->vkContext, sizeof(u32) * indexCount, HTW_BUFFER_USAGE_INDEX);

    // fill index buffer (same for all buffers in a pool)
    u32 *indexBuffer = (u32*)tc->textModel.indexBuffer->hostData;
    u32 tIndex = 0;
    for (int v = 0; v < vertsPerString; v += 4) {
        indexBuffer[tIndex++] = v + 0;
        indexBuffer[tIndex++] = v + 1;
        indexBuffer[tIndex++] = v + 2;
        indexBuffer[tIndex++] = v + 1;
        indexBuffer[tIndex++] = v + 3;
        indexBuffer[tIndex++] = v + 2;
    }

    // setup shaders
    htw_ShaderInputInfo vertInfo = {
        .size = sizeof(TextBufferData),
        .offset = 0
    };
    htw_ShaderSet shaderInfo = {
        .vertexShader = htw_loadShader(graphics->vkContext, "shaders/text.vert.spv"),
        .fragmentShader = htw_loadShader(graphics->vkContext, "shaders/text.frag.spv"),
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

static void updateTextDescriptors(kd_GraphicsState *graphics) {
    kd_TextRenderContext *tc = &graphics->textRenderContext;

    htw_updateTextDescriptor(graphics->vkContext, tc->textPipelineDescriptorSet, *tc->uniformBuffer, tc->glyphBitmap);
    // TODO: does anything below this line need to be in this method?
    htw_updateBuffer(graphics->vkContext, tc->bitmapBuffer);
    htw_updateTexture(graphics->vkContext, *tc->bitmapBuffer, tc->glyphBitmap);

    htw_updateBuffer(graphics->vkContext, tc->textModel.indexBuffer);
}

static kd_TextPoolItemHandle aquireTextPoolItem(kd_TextRenderContext *tc) {
    // TODO: determine next free pool item
    return 0;
}

static void updateTextBuffer(kd_GraphicsState *graphics, kd_TextPoolItemHandle poolItem, char *text) {
    kd_TextRenderContext *tc = &graphics->textRenderContext;

    // fill buffer
    size_t bufferOffset = poolItem * MAX_TEXT_LENGTH;
    TextBufferData *vertexBuffer = (TextBufferData*)tc->textModel.vertexBuffer->hostData + bufferOffset;
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
            kd_GlyphMetrics gm = tc->glyphMetrics[c];
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

    htw_updateBuffer(graphics->vkContext, tc->textModel.vertexBuffer); // TODO: update only the subset of the buffer that changed
}

static void setTextTransform(kd_TextRenderContext *tc, kd_TextPoolItemHandle poolItem, mat4x4 modelMatrix) {
    // TODO
    memcpy(tc->textPool[poolItem].modelMatrix, modelMatrix, sizeof(mat4x4));
}

static void freeTextPoolItem(kd_TextRenderContext *tc, kd_TextPoolItemHandle poolItem) {
    // TODO
}

static void drawText(kd_GraphicsState *graphics) {
    kd_TextRenderContext *tc = &graphics->textRenderContext;

    // TEST
    memcpy(tc->uniformBuffer->hostData, tc->textPool[0].modelMatrix, sizeof(mat4x4));
    htw_updateBuffer(graphics->vkContext, tc->uniformBuffer);

    htw_bindPipeline(graphics->vkContext, tc->textPipeline);
    htw_bindDescriptorSet(graphics->vkContext, tc->textPipeline, tc->textPipelineDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_PIPELINE);
    htw_drawPipeline(graphics->vkContext, tc->textPipeline, &tc->textModel, HTW_DRAW_TYPE_INDEXED);
}

static void initTerrainGraphics(kd_GraphicsState *graphics, kd_WorldState *world) {
    graphics->surfaceTerrain.closestChunks = malloc(sizeof(u32) * MAX_VISIBLE_CHUNKS);
    graphics->surfaceTerrain.loadedChunks = malloc(sizeof(u32) * MAX_VISIBLE_CHUNKS);
    for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++) {
        graphics->surfaceTerrain.loadedChunks[i] = -1;
    }
    createHexmapMesh(graphics, world, &graphics->surfaceTerrain);
    //createHexmapInstanceBuffers(graphics, world);
}

static void mapCamera(kd_GraphicsState *graphics, htw_PipelineHandle pipeline) {
    htw_mapPipelinePushConstant(graphics->vkContext, pipeline, &graphics->camera);
}

int kd_renderFrame(kd_GraphicsState *graphics, kd_UiState *ui, kd_WorldState *world) {

    // determine the indicies of chunks closest to the camera
    u32 centerChunk = kd_getChunkIndexByWorldPosition(world, ui->cameraX, ui->cameraY);
    updateHexmapVisibleChunks(graphics, world, &graphics->surfaceTerrain, centerChunk);

    // translate camera parameters to uniform buffer info + projection and view matricies TODO: camera position interpolation, from current to target
    vec4 cameraPosition;
    vec4 cameraFocalPoint;
    {
        float pitch = ui->cameraPitch * DEG_TO_RAD;
        float yaw = ui->cameraYaw * DEG_TO_RAD;
        float correctedDistance = powf(ui->cameraDistance, 2.0);
        float radius = cos(pitch) * correctedDistance;
        float height = sin(pitch) * correctedDistance;
        float floor = ui->cameraElevation; //ui->activeLayer == KD_WORLD_LAYER_SURFACE ? 0 : -8;
        cameraPosition = (vec4){
            .x = ui->cameraX + radius * sin(yaw),
            .y = ui->cameraY + radius * -cos(yaw),
            .z = floor + height
        };
        cameraFocalPoint = (vec4){
            .x = ui->cameraX,
            .y = ui->cameraY,
            .z = floor
        };
        updateWindowInfoBuffer(graphics, ui->mouse.x, ui->mouse.y, cameraPosition, cameraFocalPoint);

        if (ui->cameraMode == KD_CAMERA_MODE_ORTHOGRAPHIC) {
            mat4x4Orthographic(graphics->camera.projection, ((float)graphics->width / graphics->height) * correctedDistance, correctedDistance, 0.1f, 1000.f);
        } else {
            mat4x4Perspective(graphics->camera.projection, PI / 3.f, (float)graphics->width / graphics->height, 0.1f, 1000.f);
        }
        mat4x4LookAt(graphics->camera.view,
                    cameraPosition.xyz,
                    cameraFocalPoint.xyz,
                    (vec3){ {0.f, 0.f, 1.f} });
    }
    PushConstantData mvp;
    mat4x4MultiplyMatrix(mvp.pv, graphics->camera.view, graphics->camera.projection);
    mat4x4Identity(mvp.m); // remember to reinitialize those matricies every frame

    htw_beginFrame(graphics->vkContext);
    // TODO: if a pipeline layout must be used to bind descriptor sets, create some default pipeline that can be used for per-frame and per-pass bindings
    htw_bindDescriptorSet(graphics->vkContext, graphics->textRenderContext.textPipeline, graphics->perFrameDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_FRAME);
    htw_bindDescriptorSet(graphics->vkContext, graphics->textRenderContext.textPipeline, graphics->perPassDescriptorSet, HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_PASS);

    htw_bindPipeline(graphics->vkContext, graphics->surfaceTerrain.pipeline);
    // draw full terrain mesh
    for (int c = 0; c < MAX_VISIBLE_CHUNKS; c++) {
        //mat4x4SetTranslation(model, );
        u32 chunkIndex = graphics->surfaceTerrain.loadedChunks[c];
        static float chunkX, chunkY;
        kd_getChunkRootPosition(world, chunkIndex, &chunkX, &chunkY);

        // FIXME: quick hack to get world wrapping; need to wrap camera position too, shader position derivatives get weird far from the origin
        float worldDistance = (cameraFocalPoint.x - chunkX) / world->worldWidth;
        worldDistance = roundf(worldDistance);
        chunkX += worldDistance * world->worldWidth;

        mat4x4SetTranslation(mvp.m, (vec3){{chunkX, chunkY, 0.0}});
        htw_pushConstants(graphics->vkContext, graphics->surfaceTerrain.pipeline, &mvp);
        htw_bindDescriptorSet(graphics->vkContext, graphics->surfaceTerrain.pipeline, graphics->surfaceTerrain.chunkObjectDescriptorSets[c], HTW_DESCRIPTOR_BINDING_FREQUENCY_PER_OBJECT);
        htw_drawPipeline(graphics->vkContext, graphics->surfaceTerrain.pipeline, &graphics->surfaceTerrain.modelData, HTW_DRAW_TYPE_INDEXED);
    }

    // update ui with feedback info from shaders
    htw_retreiveBuffer(graphics->vkContext, graphics->feedbackInfoBuffer);
    FeedbackInfo *feedbackData = graphics->feedbackInfoBuffer->hostData;
    u32 hoveredChunk = feedbackData->chunkIndex;
    u32 hoveredCell = feedbackData->cellIndex;
    ui->hoveredChunkIndex = hoveredChunk;
    ui->hoveredCellIndex = hoveredCell;

    // draw text overlays
    kd_TextPoolItemHandle textItem = aquireTextPoolItem(&graphics->textRenderContext);
    char statusText[256];
    sprintf(statusText,
            "Mouse Position: %4.4i, %4.4i\nChunk Index: %4.4u\nHighlighted Cell: %4.4u",
            ui->mouse.x, ui->mouse.y, hoveredChunk, hoveredCell);
    updateTextBuffer(graphics, textItem, statusText);
    mat4x4 textTransform;
    // NOTE: ccVector's matnxnSet* methods start from a new identity matrix and overwrite the *entire* input matrix
    // NOTE: remember that the order of translate/rotate/scale matters (you probably want to scale first)
    mat4x4SetScale(textTransform, 1.0); // TODO: figure out how to resolve bleed from neighboring glyphs when scaling text mesh
    mat4x4Translate(textTransform, (vec3){{-1.0, -1.0, 0.0}}); // Position in top-left corner
    setTextTransform(&graphics->textRenderContext, textItem, textTransform);
    drawText(graphics);

//     drawText(graphics, "1234567890_abcdefghijklmnopqrstuvwxyz", (vec3){{-600.f, -40.f, 0.f}});
//     char allChars[128];
//     for (int i = 0; i < 128; i++) {
//         allChars[i] = (char)i;
//     }
//     allChars[0] = '0';
//     drawText(graphics, allChars, (vec3){{-1280.f, 0.f, 0.f}});

    // draw instanced hexagons
    if (ui->activeLayer == KD_WORLD_LAYER_SURFACE) {
        //htw_drawPipeline(graphics->vkContext, graphics->instanceTerrain.pipeline, graphics->instanceTerrain.modelData, HTW_DRAW_TYPE_INSTANCED);
    }
//     else if (ui->activeLayer == KD_WORLD_LAYER_CAVE) {
//         htw_drawPipeline(graphics->vkContext, graphics->pipeline, graphics->caveInstanceBuffer, 54, instanceCount);
//     }
    htw_endFrame(graphics->vkContext);

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
     * hexagon edges are connected to each other by a grid of quads
     *
     * between 3 edge connection grids, in the corner joining 3 tiles, is a triangular area
     * (also use subdivisions for edges and corners?)
     *
     * The mesh has one extra tri strip on the top and right sides, so that the gaps between chunks can be filled seamlessly
     */

    // create rendering pipeline
    htw_ShaderInputInfo positionInputInfo = {
        .size = sizeof(((kd_HexmapVertexData*)0)->position), // pointer casting trick to get size of a struct member
        .offset = offsetof(kd_HexmapVertexData, position)
    };
    htw_ShaderInputInfo cellInputInfo = {
        .size = sizeof(((kd_HexmapVertexData*)0)->cellIndex),
        .offset = offsetof(kd_HexmapVertexData, cellIndex)
    };
    htw_ShaderInputInfo vertexInputInfos[] = {positionInputInfo, cellInputInfo};
    htw_ShaderSet shaderInfo = {
        .vertexShader = htw_loadShader(graphics->vkContext, "shaders/hexTerrain.vert.spv"),
        .fragmentShader = htw_loadShader(graphics->vkContext, "shaders/hexTerrain.frag.spv"),
        .vertexInputStride = sizeof(kd_HexmapVertexData),
        .vertexInputCount = 2,
        .vertexInputInfos = vertexInputInfos,
        .instanceInputCount = 0,
    };

    terrain->chunkObjectLayout = htw_createTerrainObjectSetLayout(graphics->vkContext);
    htw_DescriptorSetLayout terrainPipelineLayouts[] = {graphics->perFrameLayout, graphics->perPassLayout, NULL, terrain->chunkObjectLayout};
    terrain->pipeline = htw_createPipeline(graphics->vkContext, terrainPipelineLayouts, shaderInfo);

    // create descriptor set for each chunk, to swap between during each frame
    terrain->chunkObjectDescriptorSets = malloc(sizeof(htw_DescriptorSet) * MAX_VISIBLE_CHUNKS);
    htw_allocateDescriptors(graphics->vkContext, terrain->chunkObjectLayout, MAX_VISIBLE_CHUNKS, terrain->chunkObjectDescriptorSets);

    mapCamera(graphics, terrain->pipeline);

    // determine required size for vertex and triangle buffers
    const u32 width = world->chunkWidth;
    const u32 height = world->chunkHeight;
    const u32 cellCount = width * height;
    const u32 vertsPerCell = 7;
    // for connecting to adjacent chunks
    const u32 rightSideVerts = height * 3;
    const u32 topSideVerts = width * 3;
    const u32 topRightCornerVerts = 1;
    const u32 vertexCount = (vertsPerCell * cellCount) + rightSideVerts + topSideVerts + topRightCornerVerts;
    const u32 vertexDataSize = sizeof(kd_HexmapVertexData);
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
    htw_ModelData modelData = {
        .vertexBuffer = getNextBuffer(&graphics->bufferPool),
        .indexBuffer = getNextBuffer(&graphics->bufferPool),
        .vertexCount = vertexCount,
        .indexCount = triangleCount
    };
    *modelData.vertexBuffer = htw_createBuffer(graphics->vkContext, vertexDataSize * vertexCount, HTW_BUFFER_USAGE_VERTEX);
    *modelData.indexBuffer = htw_createBuffer(graphics->vkContext, sizeof(u32) * triangleCount, HTW_BUFFER_USAGE_INDEX);
    terrain->modelData = modelData;
    // fill mesh buffer data
    kd_HexmapVertexData *vertexData = modelData.vertexBuffer->hostData;
    u32 *triData = modelData.indexBuffer->hostData;
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

    // best way to handle using multiple sub-buffers and binding a different one each draw call?
    // Options:
    // a. create a descriptor set for every sub buffer, and set a different buffer offset for each. When drawing, most descriptors sets can be bound once per frame. A new terrain data descriptor set will be bound before every draw call.
    // b. create one descriptor set with a dynamic storage buffer. When drawing, bind the terrain data descriptor set with a different dynamic offset before every draw call.
    // because I don't need to change the size of these sub buffers at runtime, there shouldn't be much of a difference between these approaches.
    // include strips of adjacent chunk data in buffer size
    u32 bufferCellCount = cellCount + width + height + 1;
    // Chunk data includes the chunk index out front
    terrain->terrainChunkHostSize = sizeof(u32) + (sizeof(TerrainBufferData) * bufferCellCount);
    terrain->terrainDataBuffer = getNextBuffer(&graphics->bufferPool);
    *terrain->terrainDataBuffer = htw_createSplitBuffer(graphics->vkContext, terrain->terrainChunkHostSize, MAX_VISIBLE_CHUNKS, HTW_BUFFER_USAGE_STORAGE, &terrain->terrainChunkDeviceSize);

    // create and write model data for each cell
    // vert and tri array layout: | main chunk data (left to right, bottom to top) | right chunk edge strip (bottom to top) | top chunk edge strip (left to right) | top-right corner infill |
    u32 vIndex = 0;
    u32 tIndex = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned int c = x + (y * width); // current cell
            // vertex data
            for (int v = 0; v < vertsPerCell; v++) {
                float posX, posY;
                htw_geo_getHexCellPositionSkewed(x, y, &posX, &posY);
                vec3 pos = vec3Add(hexagonPositions[v], (vec3){{posX, posY, 0.0}});
                kd_HexmapVertexData newVertex = {
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
        // vertex data; from bottom-left to top vertex
        for (int v = 0; v < 3; v++) {
            float posX, posY;
            htw_geo_getHexCellPositionSkewed(x + 1, y, &posX, &posY);
            u32 hexPosIndex = leftEdgeConnectionVerts[v];
            vec3 pos = vec3Add(hexagonPositions[hexPosIndex], (vec3){{posX, posY, 0.0}});
            kd_HexmapVertexData newVertex = {
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
        // vertex data; from bottom-left to top vertex
        for (int v = 0; v < 3; v++) {
            float posX, posY;
            htw_geo_getHexCellPositionSkewed(x, y + 1, &posX, &posY);
            u32 hexPosIndex = topEdgeConnectionVerts[v];
            vec3 pos = vec3Add(hexagonPositions[hexPosIndex], (vec3){{posX, posY, 0.0}});
            kd_HexmapVertexData newVertex = {
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
            float posX, posY;
            htw_geo_getHexCellPositionSkewed(x + 1, y + 1, &posX, &posY);
            vec3 pos = vec3Add(hexagonPositions[5], (vec3){{posX, posY, 0.0}});
            kd_HexmapVertexData newVertex = {
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

static void updateHexmapDescriptors(kd_GraphicsState *graphics, kd_HexmapTerrain *terrain){
    // TEST
    htw_updateTerrainObjectDescriptors(graphics->vkContext, terrain->chunkObjectDescriptorSets, MAX_VISIBLE_CHUNKS, terrain->terrainChunkDeviceSize, *terrain->terrainDataBuffer);
}

static void updateHexmapVisibleChunks(kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain, u32 centerChunk) {
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
    s32 *closestChunks = graphics->surfaceTerrain.closestChunks;
    s32 *loadedChunks = graphics->surfaceTerrain.loadedChunks;
    for (int c = 0, y = 1 - MAX_VISIBLE_CHUNK_DISTANCE; y < MAX_VISIBLE_CHUNK_DISTANCE; y++) {
        for (int x = 1 - MAX_VISIBLE_CHUNK_DISTANCE; x < MAX_VISIBLE_CHUNK_DISTANCE; x++, c++) {
            closestChunks[c] = kd_getChunkIndexAtOffset(world, centerChunk, x, y);
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
    for (int i = 0; i < MAX_VISIBLE_CHUNKS; i++) {
        s32 loadedTarget = loadedChunks[i];
        u8 foundMatch = 0;
        for (int k = 0; k < MAX_VISIBLE_CHUNKS; k++) {
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
    while (ia < MAX_VISIBLE_CHUNKS) {
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

    for (int c = 0; c < MAX_VISIBLE_CHUNKS; c++) {
        // TODO: only update chunk if freshly visible or has pending updates
        // TODO: also find some way to avoid loading duplicate chunks when near the top or bottom of the world (causes flickering as higher chunks are moved down)
        updateHexmapDataBuffer(graphics, world, &graphics->surfaceTerrain, loadedChunks[c], c);
    }
}

static void updateHexmapDataBuffer (kd_GraphicsState *graphics, kd_WorldState *world, kd_HexmapTerrain *terrain, u32 chunkIndex, u32 subBufferIndex) {
    const u32 width = world->chunkWidth;
    const u32 height = world->chunkHeight;
    kd_MapChunk *chunk = &world->chunks[chunkIndex];

    size_t subBufferHostOffset = subBufferIndex * terrain->terrainChunkHostSize;
    size_t subBufferDeviceOffset = subBufferIndex * terrain->terrainChunkDeviceSize;
    u32 *bufferIndex = terrain->terrainDataBuffer->hostData + subBufferHostOffset; // address of chunk index
    TerrainBufferData *bufferData = (TerrainBufferData*)(bufferIndex + 1); // address of terrain data array

    *bufferIndex = chunkIndex;

    // get primary chunk data
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            u32 c = x + (y * width);
            setHexmapBufferFromCell(chunk, x, y, &bufferData[c]);
        }
    }

    u32 edgeDataIndex = width * height;
    // get chunk data from chunk at x + 1
    u32 rightChunk = kd_getChunkIndexAtOffset(world, chunkIndex, 1, 0);
    chunk = &world->chunks[rightChunk];
    for (u32 y = 0; y < height; y++) {
        u32 c = edgeDataIndex + y; // right edge of this row
        setHexmapBufferFromCell(chunk, 0, y, &bufferData[c]);
    }
    edgeDataIndex += height;

    // get chunk data from chunk at y + 1
    u32 topChunk = kd_getChunkIndexAtOffset(world, chunkIndex, 0, 1);
    chunk = &world->chunks[topChunk];
    for (u32 x = 0; x < width; x++) {
        u32 c = edgeDataIndex + x;
        setHexmapBufferFromCell(chunk, x, 0, &bufferData[c]);
    }
    edgeDataIndex += width;

    // get chunk data from chunk at x+1, y+1 (only one cell)
    u32 topRightChunk = kd_getChunkIndexAtOffset(world, chunkIndex, 1, 1);
    chunk = &world->chunks[topRightChunk];
    setHexmapBufferFromCell(chunk, 0, 0, &bufferData[edgeDataIndex]); // set last position in buffer

    htw_updateSubBuffer(graphics->vkContext, terrain->terrainDataBuffer, subBufferHostOffset, subBufferDeviceOffset, terrain->terrainChunkDeviceSize);
}

static void setHexmapBufferFromCell(kd_MapChunk *chunk, u32 cellX, u32 cellY, TerrainBufferData *target) {
    s32 rawElevation = htw_geo_getMapValue(chunk->heightMap, cellX, cellY);
    s32 rawTemperature = htw_geo_getMapValue(chunk->temperatureMap, cellX, cellY);
    s32 rawRainfall = htw_geo_getMapValue(chunk->rainfallMap, cellX, cellY);
    TerrainBufferData cellData = {
        .elevation = rawElevation,
        .paletteX = (u8)remap_int(rawTemperature, 0, chunk->temperatureMap->maxMagnitude, 0, 255),
        .paletteY = (u8)remap_int(rawRainfall, 0, chunk->rainfallMap->maxMagnitude, 0, 255),
    };
    *target = cellData;

}

// static vec3 getRandomColor() {
//     float r = (float)rand() / (float)RAND_MAX;
//     float g = (float)rand() / (float)RAND_MAX;
//     float b = (float)rand() / (float)RAND_MAX;
//     return (vec3){ {r, g, b} };
// }