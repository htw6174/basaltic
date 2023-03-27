#ifndef BASALTIC_COMPONENTS_H_INCLUDED
#define BASALTIC_COMPONENTS_H_INCLUDED

#include "htw_core.h"
#include "flecs.h"
#include "components/basaltic_character.h"
#include "components/basaltic_terrain.h"

/* Terrain */
extern ECS_COMPONENT_DECLARE(bc_TerrainMap);
extern ECS_TAG_DECLARE(Surface);
extern ECS_TAG_DECLARE(Subterranian);
extern ECS_TAG_DECLARE(IsOn);

// For marking entities that contain multiple child entities occupying the same cell
extern ECS_TAG_DECLARE(CellRoot);

/* Character */
extern ECS_COMPONENT_DECLARE(bc_Stamina);

extern ECS_COMPONENT_DECLARE(bc_GridPosition);
extern ECS_COMPONENT_DECLARE(bc_GridDestination);

extern ECS_TAG_DECLARE(PlayerControlled);
extern ECS_TAG_DECLARE(PlayerVision);
extern ECS_TAG_DECLARE(BehaviorWander);
extern ECS_TAG_DECLARE(BehaviorDescend);

void bc_defineComponents(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_H_INCLUDED
