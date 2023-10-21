#ifndef BASALTIC_COMPONENTS_H_INCLUDED
#define BASALTIC_COMPONENTS_H_INCLUDED

#include "flecs.h"
#include "components/bc_components_common.h"
#include "components/basaltic_components_planes.h"
#include "components/basaltic_components_actors.h"
#include "components/basaltic_components_wildlife.h"
#include "components/bc_components_elementals.h"

#undef ECS_META_IMPL
#ifndef BASALTIC_COMPONENTS_IMPL
#define ECS_META_IMPL EXTERN // Ensure meta symbols are only defined once
#endif

// TODO: find a home for this
extern ECS_TAG_DECLARE(PlayerVision);

void BcImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_H_INCLUDED
