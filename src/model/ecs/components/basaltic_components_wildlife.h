#ifndef BASALTIC_COMPONENTS_WILDLIFE_H_INCLUDED
#define BASALTIC_COMPONENTS_WILDLIFE_H_INCLUDED

#include "flecs.h"

// Alternative: could define in terms of foraging style. Grazer, Browser, Digger, etc.
extern ECS_TAG_DECLARE(Diet); // Relationship
// OneOf Targets
extern ECS_TAG_DECLARE(Grasses);
extern ECS_TAG_DECLARE(Foliage);
extern ECS_TAG_DECLARE(Fruit);
extern ECS_TAG_DECLARE(Meat);

extern ECS_TAG_DECLARE(EgoGrazer);
extern ECS_TAG_DECLARE(EgoPredator);
extern ECS_TAG_DECLARE(EgoHunter);

extern ECS_TAG_DECLARE(ActionFeed);
extern ECS_TAG_DECLARE(ActionFollow);
extern ECS_TAG_DECLARE(ActionAttack);

extern ECS_TAG_DECLARE(Flying); // Ignore terrain when moving
extern ECS_TAG_DECLARE(Climbing); // Reduced stamina penalty for elevation difference when moving
extern ECS_TAG_DECLARE(Amphibious); // Move through bodies of water with no penalty
extern ECS_TAG_DECLARE(Aquatic); // Can only move along bodies of water

void BcWildlifeImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_WILDLIFE_H_INCLUDED
