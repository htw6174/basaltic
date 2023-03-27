/*
 * TODO: code generation for this file. Should be possible to derive everything here from basaltic_components.h
 * Only the non-extern declarations and simple definitions are setup here
 */
#include "basaltic_components.h"
#include "flecs.h"

/* Terrain */
ECS_COMPONENT_DECLARE(bc_TerrainMap);
ECS_TAG_DECLARE(Surface);
ECS_TAG_DECLARE(Subterranian);
ECS_TAG_DECLARE(IsOn);

ECS_TAG_DECLARE(CellRoot);

/* Character */
ECS_COMPONENT_DECLARE(bc_Stamina);

ECS_COMPONENT_DECLARE(bc_GridPosition);
ECS_COMPONENT_DECLARE(bc_GridDestination);

ECS_TAG_DECLARE(PlayerControlled);
ECS_TAG_DECLARE(PlayerVision);
ECS_TAG_DECLARE(BehaviorWander);
ECS_TAG_DECLARE(BehaviorDescend);

void bc_defineComponents(ecs_world_t *world) {

    /* Terrain */
    ECS_COMPONENT_DEFINE(world, bc_TerrainMap);
    ECS_TAG_DEFINE(world, Surface);
    ECS_TAG_DEFINE(world, Subterranian);
    ECS_TAG_DEFINE(world, IsOn);

    ECS_TAG_DEFINE(world, CellRoot);

    /* Character */
    ECS_COMPONENT_DEFINE(world, bc_Stamina);

    ECS_COMPONENT_DEFINE(world, bc_GridPosition);
    ECS_COMPONENT_DEFINE(world, bc_GridDestination);

    ECS_TAG_DEFINE(world, PlayerControlled);
    ECS_TAG_DEFINE(world, PlayerVision);
    ECS_TAG_DEFINE(world, BehaviorWander);
    ECS_TAG_DEFINE(world, BehaviorDescend);
}
