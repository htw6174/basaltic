#include "basaltic_systems.h"
#include "basaltic_components.h"
#include "basaltic_terrain_systems.h"
#include "basaltic_character_systems.h"

void BcSystemsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystems);

    ECS_IMPORT(world, BcSystemsTerrain);
    //ECS_IMPORT(world, BcSystemsCharacters);
}
