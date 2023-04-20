#include "basaltic_systems.h"
#include "basaltic_components.h"
#include "basaltic_terrain_systems.h"
#include "basaltic_character_systems.h"

void BasalticSystemsImport(ecs_world_t *world) {
    ECS_MODULE(world, BasalticSystems);

    ECS_IMPORT(world, BasalticSystemsTerrain);
    ECS_IMPORT(world, BasalticSystemsCharacters);
}
