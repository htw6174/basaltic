#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include "ccVector.h"
#include "htw_vulkan.h"
#include "kingdom_window.h"
#include "kingdom_logicInputState.h"
#include "kingdom_worldState.h"

htw_VkContext *kd_createWindow() {
    SDL_Window *sdlWindow = SDL_CreateWindow("kingdom", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, HTW_VK_WINDOW_FLAGS);
    htw_VkContext *context = htw_createVkContext(sdlWindow);
    return context;
}

htw_PipelineHandle kd_createPipeline(htw_VkContext *vkContext) {
    htw_ShaderHandle vert = htw_loadShader(vkContext, "shaders/hexagon.vert.spv");
    htw_ShaderHandle frag = htw_loadShader(vkContext, "shaders/hexagon.frag.spv");
    return htw_createPipeline(vkContext, vert, frag);
}

void kd_createTerrainBuffer(kd_GraphicsState *graphics, kd_WorldState *world) {
    int width = world->heightMap->width;
    int height = world->heightMap->height;
    uint32_t tileCount = width * height;
    uint32_t size = sizeof(vec4) * tileCount;

    graphics->surfaceBuffer = htw_createBuffer(graphics->vkContext, size);
    graphics->caveBuffer = htw_createBuffer(graphics->vkContext, size);
    vec4 *surfacePositions = (vec4*)graphics->surfaceBuffer.hostData;
    vec4 *cavePositions = (vec4*)graphics->caveBuffer.hostData;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float worldX;
            float worldY;
            htw_getHexCellPositionSkewed(x, y, &worldX, &worldY);
            float height = (float)world->heightMap->values[x + (y * width)] / 10.f;
            vec4 s = {
                .x = worldX,
                .y = worldY,
                .z = height,
                .w = 0
            };
            vec4 c = {
                .x = worldX,
                .y = worldY,
                .z = -height,
                .w = 0
            };
            surfacePositions[x + (y * width)] = s;
            cavePositions[x + (y * width)] = c;
        }
    }
    htw_finalizeBuffers(graphics->vkContext, 2, &graphics->surfaceBuffer);
    htw_updateBuffer(graphics->vkContext, &graphics->surfaceBuffer);
    htw_updateBuffer(graphics->vkContext, &graphics->caveBuffer);
}

void kd_mapCamera(kd_GraphicsState *graphics) {
    htw_mapPipelinePushConstant(graphics->vkContext, graphics->pipeline, &graphics->camera);
}

int kd_renderFrame(kd_GraphicsState *graphics, kd_UiState *ui, kd_WorldState *world) {
    //bmp = valueMapToBitmap(world->heightMap, bmp, terrainFormat);

    uint32_t instanceCount = world->heightMap->width * world->heightMap->height;
    //instanceCount = min_int(instanceCount, graphics->frame % instanceCount);

    // translate camera parameters to projection and view matricies TODO: camera position interpolation, from current to target
    {
        float pitch = ui->cameraPitch * DEG_TO_RAD;
        float yaw = ui->cameraYaw * DEG_TO_RAD;
        float radius = cos(pitch) * ui->cameraDistance;
        float height = sin(pitch) * ui->cameraDistance;
        float floor = 0; //ui->activeLayer == KD_WORLD_LAYER_SURFACE ? 0 : -8;
        if (ui->cameraMode == KD_CAMERA_MODE_ORTHOGRAPHIC) {
            mat4x4Orthographic(graphics->camera.projection, 8 * ui->cameraDistance, 6 * ui->cameraDistance, 0.1f, 1000.f);
        }
        else {
            mat4x4Perspective(graphics->camera.projection, PI / 3.f, 800.f / 600.f, 0.1f, 1000.f);
        }
        mat4x4LookAt(graphics->camera.view,
                    (vec3){ {ui->cameraX + radius * sin(yaw), ui->cameraY + radius * -cos(yaw), floor + height} },
                    (vec3){ {ui->cameraX, ui->cameraY, floor} },
                    (vec3){ {0.f, 0.f, 1.f} });
    }

    htw_Frame frame = htw_beginFrame(graphics->vkContext);
    if (ui->activeLayer == KD_WORLD_LAYER_SURFACE) {
        htw_drawPipeline(graphics->vkContext, frame, graphics->pipeline, graphics->surfaceBuffer, 54, instanceCount);
    }
    else if (ui->activeLayer == KD_WORLD_LAYER_CAVE) {
        htw_drawPipeline(graphics->vkContext, frame, graphics->pipeline, graphics->caveBuffer, 54, instanceCount);
    }
    htw_endFrame(graphics->vkContext, frame);

    return 0;
}
