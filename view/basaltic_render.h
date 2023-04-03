#ifndef BASALTIC_RENDER_H_INCLUDED
#define BASALTIC_RENDER_H_INCLUDED

#include "htw_core.h"
#include "htw_vulkan.h"
#include "ccVector.h"
#include "basaltic_window.h"
#include "basaltic_uiState.h"
#include "basaltic_logic.h"
#include "flecs.h"

typedef struct {
    mat4x4 projection;
    mat4x4 view;
} bc_Camera;

typedef struct {
    vec2 windowSize;
    vec2 mousePosition;
    // only xyz used, but needs to be vec4 for shader uniform buffer alignment
    vec4 cameraPosition;
    vec4 cameraFocalPoint;
    float visibilityRadius;
    float fogExtinction;
    float fogInscattering;
} bc_WindowInfo;

typedef struct {
    u32 chunkIndex;
    u32 cellIndex;
} bc_FeedbackInfo;

typedef struct {
    vec3 gridToWorld;
    float totalWidth;
    u32 timeOfDay;
    s32 seaLevel;
    u32 visibilityOverrideBits;
} bc_WorldInfo;

typedef struct private_bc_RenderContext *bc_InternalRenderContext;

typedef struct {
    htw_VkContext *vkContext;
    htw_BufferPool bufferPool;

    htw_DescriptorSetLayout perFrameLayout;
    htw_DescriptorSet perFrameDescriptorSet;
    htw_DescriptorSetLayout perPassLayout;
    htw_DescriptorSet perPassDescriptorSet;

    bc_WindowInfo windowInfo;
    bc_FeedbackInfo feedbackInfo;
    bc_WorldInfo worldInfo;
    htw_Buffer windowInfoBuffer;
    htw_Buffer feedbackInfoBuffer; // Storage buffer written to by the fragment shader to find which cell the mouse is over
    htw_Buffer worldInfoBuffer;

    u32 chunkVisibilityRadius;

    bc_Camera camera;

    // TEST
    bool drawSystems;

    htw_PipelineHandle defaultPipeline;

    vec3 wrapInstancePositions[4];

    bc_InternalRenderContext internalRenderContext;
} bc_RenderContext;

bc_RenderContext* bc_createRenderContext(bc_WindowContext* wc);

void bc_setWorldRenderScale(bc_RenderContext *rc, vec3 worldScale);
void bc_updateRenderContextWithWorldParams(bc_RenderContext *rc, bc_WorldState *world);
void bc_updateRenderContextWithUiState(bc_RenderContext *rc, bc_WindowContext *wc, bc_UiState *ui);

void bc_renderFrame(bc_RenderContext *rc, bc_WorldState *world);

void bc_setRenderWorldWrap(bc_RenderContext *rc, u32 worldWidth, u32 worldHeight);

#endif // BASALTIC_RENDER_H_INCLUDED
