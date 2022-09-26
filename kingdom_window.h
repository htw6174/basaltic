#ifndef KINGDOM_MAIN_H_INCLUDED
#define KINGDOM_MAIN_H_INCLUDED

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "ccVector.h"
#include "htw_vulkan.h"
#include "kingdom_uiState.h"
#include "kingdom_worldState.h"

#define PI 3.141592f
#define DEG_TO_RAD 0.017453f

typedef struct {
    mat4x4 projection;
    mat4x4 view;
} kd_Camera;

typedef struct kd_BufferPool {
    htw_Buffer *buffers;
    unsigned int count;
    unsigned int maxCount;
} kd_BufferPool;

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

typedef struct {
    unsigned int width, height;
    uint64_t milliSeconds;
    uint64_t frame;
    // TODO: time of last frame or deltatime?
    htw_VkContext *vkContext;
    kd_BufferPool bufferPool;
    kd_HexmapTerrain surfaceTerrain;
    kd_HexmapTerrain caveTerrain;
    kd_InstanceTerrain instanceTerrain;
    kd_Camera camera;
} kd_GraphicsState;

void kd_InitGraphics(kd_GraphicsState *graphics, unsigned int width, unsigned int height);
void kd_createTerrainBuffer(kd_GraphicsState *graphics, kd_WorldState *world);
int kd_renderFrame(kd_GraphicsState *graphics, kd_UiState *ui, kd_WorldState *world);

#endif // KINGDOM_MAIN_H_INCLUDED
