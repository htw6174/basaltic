#ifndef BASALTIC_COMPONENTS_WILDLIFE_H_INCLUDED
#define BASALTIC_COMPONENTS_WILDLIFE_H_INCLUDED

#include "flecs.h"

#undef ECS_META_IMPL
#undef BC_DECL
#ifndef BC_COMPONENT_IMPL
#define ECS_META_IMPL EXTERN
#define BC_DECL extern
#else
#define BC_DECL
#endif

// Alternative: could define in terms of foraging style. Grazer, Browser, Digger, etc.
BC_DECL ECS_TAG_DECLARE(Diet); // Relationship
// OneOf Targets
BC_DECL ECS_TAG_DECLARE(Grasses);
BC_DECL ECS_TAG_DECLARE(Foliage);
BC_DECL ECS_TAG_DECLARE(Fruit);
BC_DECL ECS_TAG_DECLARE(Meat);

BC_DECL ECS_TAG_DECLARE(EgoGrazer);
BC_DECL ECS_TAG_DECLARE(EgoPredator);
BC_DECL ECS_TAG_DECLARE(EgoHunter);

BC_DECL ECS_TAG_DECLARE(ActionFeed);
BC_DECL ECS_TAG_DECLARE(ActionFollow);
BC_DECL ECS_TAG_DECLARE(ActionAttack);

BC_DECL ECS_TAG_DECLARE(Flying); // Ignore terrain when moving
BC_DECL ECS_TAG_DECLARE(Climbing); // Reduced stamina penalty for elevation difference when moving
BC_DECL ECS_TAG_DECLARE(Amphibious); // Move through bodies of water with no penalty
BC_DECL ECS_TAG_DECLARE(Aquatic); // Can only move along bodies of water

void BcWildlifeImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_WILDLIFE_H_INCLUDED
