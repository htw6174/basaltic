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

typedef struct kd_HexmapTerrain {
    ValueMap *heightMap;
    htw_Buffer vertexBuffer;
    uint32_t *tileVertexIndicies;
} kd_HexmapTerrain;

typedef struct {
    uint64_t milliSeconds;
    uint64_t frame;
    // TODO: time of last frame or deltatime?
    htw_VkContext *vkContext;
    htw_PipelineHandle pipeline;
    htw_Buffer surfaceBuffer;
    htw_Buffer caveBuffer;
    kd_Camera camera;
} kd_GraphicsState;

htw_VkContext *kd_createWindow();
htw_PipelineHandle kd_createPipeline(htw_VkContext *vkContext);
void kd_createTerrainBuffer(kd_GraphicsState *graphics, kd_WorldState *world);
void kd_mapCamera(kd_GraphicsState *graphics);
int kd_renderFrame(kd_GraphicsState *graphics, kd_UiState *ui, kd_WorldState *world);

#endif // KINGDOM_MAIN_H_INCLUDED
