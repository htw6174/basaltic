#ifndef BASALTIC_GRAPHICS_H_INCLUDED
#define BASALTIC_GRAPHICS_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "ccVector.h"
#include "basaltic_render.h"
#include "basaltic_view.h"
#include "basaltic_model.h"
#include "basaltic_uiState.h"

typedef struct {
    u32 maxInstanceCount;
    htw_PipelineHandle pipeline;
    htw_DescriptorSetLayout debugLayout;
    htw_DescriptorSet debugDescriptorSet;
    bc_Model instancedModel;
} bc_DebugSwarm;

typedef struct {


    bc_TextRenderContext textRenderContext;
    bc_HexmapTerrain surfaceTerrain;
    bc_HexmapTerrain caveTerrain;
    bool showCharacterDebug;
    bc_DebugSwarm characterDebug;
} bc_GraphicsState;

bc_GraphicsState* bc_createGraphicsState();

#endif // BASALTIC_GRAPHICS_H_INCLUDED
