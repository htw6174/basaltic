#ifndef BASALTIC_CHARACTER_SYSTEMS_H_INCLUDED
#define BASALTIC_CHARACTER_SYSTEMS_H_INCLUDED

#include "htw_core.h"
#include "htw_geomap.h"
#include "flecs.h"

void bc_createCharacters(ecs_world_t *world, ecs_entity_t plane, size_t count);

void BcSystemsCharactersImport(ecs_world_t *world);

#endif // BASALTIC_CHARACTER_SYSTEMS_H_INCLUDED
