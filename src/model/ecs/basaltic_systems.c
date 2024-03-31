#include "basaltic_systems.h"
#include "basaltic_components.h"
#include "bc_systems_common.h"
#include "basaltic_terrain_systems.h"
#include "basaltic_character_systems.h"
#include "bc_elementals_systems.h"
#include "bc_systems_tribes.h"

void BcSystemsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystems);

    ECS_IMPORT(world, BcSystemsCommon);
    ECS_IMPORT(world, BcSystemsTerrain);
    ECS_IMPORT(world, BcSystemsCharacters);
    ECS_IMPORT(world, BcSystemsElementals);
    ECS_IMPORT(world, BcSystemsTribes);
}
