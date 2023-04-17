#include "basaltic_components.h"
#include "flecs.h"
#include "basaltic_phases.h"
#include "components/basaltic_components_planes.h"
#include "components/basaltic_components_actors.h"

ECS_TAG_DECLARE(PlayerVision);

void BasalticComponentsImport(ecs_world_t *world) {

    ECS_MODULE(world, BasalticComponents);

    ECS_IMPORT(world, BasalticPhases);
    ECS_IMPORT(world, BasalticComponentsPlanes);
    ECS_IMPORT(world, BasalticComponentsActors);

    ECS_TAG_DEFINE(world, PlayerVision);

    // NOTE: the 'EcsWith' relationship makes adding the source component automatically add the target component
    //ecs_add_pair(world, PlayerControlled, EcsWith, PlayerVision);
}
