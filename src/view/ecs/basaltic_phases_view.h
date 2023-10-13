#ifndef BASALTIC_PHASES_VIEW_H_INCLUDED
#define BASALTIC_PHASES_VIEW_H_INCLUDED

#include "flecs.h"

extern ECS_TAG_DECLARE(PreShadowPass);
extern ECS_TAG_DECLARE(OnShadowPass);
extern ECS_TAG_DECLARE(PostShadowPass);

extern ECS_TAG_DECLARE(PreRenderPass);
extern ECS_TAG_DECLARE(OnRenderPass);
extern ECS_TAG_DECLARE(PostRenderPass);

extern ECS_TAG_DECLARE(OnLightingPass);
extern ECS_TAG_DECLARE(OnFinalPass);

extern ECS_TAG_DECLARE(OnModelChanged);

extern ECS_DECLARE(ModelChangedPipeline);

void BcviewPhasesImport(ecs_world_t *world);

#endif // BASALTIC_PHASES_VIEW_H_INCLUDED
