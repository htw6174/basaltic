#ifndef BASALTIC_PHASES_VIEW_H_INCLUDED
#define BASALTIC_PHASES_VIEW_H_INCLUDED

#include "flecs.h"

extern ECS_TAG_DECLARE(BeginPassShadow);
extern ECS_TAG_DECLARE(OnPassShadow);
extern ECS_TAG_DECLARE(EndPassShadow);

extern ECS_TAG_DECLARE(BeginPassGBuffer);
extern ECS_TAG_DECLARE(OnPassGBuffer);
extern ECS_TAG_DECLARE(EndPassGBuffer);

extern ECS_TAG_DECLARE(BeginPassLighting);
extern ECS_TAG_DECLARE(OnPassLighting);
extern ECS_TAG_DECLARE(EndPassLighting);

extern ECS_TAG_DECLARE(BeginPassTransparent);
extern ECS_TAG_DECLARE(OnPassTransparent);
extern ECS_TAG_DECLARE(EndPassTransparent);

extern ECS_TAG_DECLARE(OnPassFinal);

extern ECS_TAG_DECLARE(OnModelChanged);

extern ECS_DECLARE(ModelChangedPipeline);

void BcviewPhasesImport(ecs_world_t *world);

#endif // BASALTIC_PHASES_VIEW_H_INCLUDED
