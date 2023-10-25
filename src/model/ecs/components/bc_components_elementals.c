#include "basaltic_components_actors.h"
#define BC_COMPONENT_IMPL
#include "bc_components_elementals.h"

void BcElementalsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcElementals);

    ECS_IMPORT(world, BcActors);

    ECS_TAG_DEFINE(world, ElementalSpirit);

    //ecs_set_scope(world, Ego); // Should create these in the right scope; consider setting up tags in plecs format?
    ecs_entity_t oldScope = ecs_get_scope(world);
    ecs_set_scope(world, Ego);
    ECS_TAG_DEFINE(world, EgoTectonicSpirit);
    ECS_TAG_DEFINE(world, EgoVolcanoSpirit);
    ECS_TAG_DEFINE(world, EgoEarthSpirit);
    ECS_TAG_DEFINE(world, EgoOceanSpirit);
    ECS_TAG_DEFINE(world, EgoStormSpirit);
    ecs_set_scope(world, oldScope);

    ecs_set_scope(world, Action);
    ECS_TAG_DEFINE(world, ActionShiftPlates);
    ECS_TAG_DEFINE(world, ActionErupt);
    ecs_set_scope(world, oldScope);

    ECS_META_COMPONENT(world, SpiritPower);
    ECS_META_COMPONENT(world, SpiritLifetime);
    ECS_META_COMPONENT(world, PlateShiftStrength);

    ECS_COMPONENT_DEFINE(world, AngleRadians);
    ecs_primitive(world, {.entity = ecs_id(AngleRadians), .kind = EcsF32});

    ECS_COMPONENT_DEFINE(world, Elevation);
    ecs_primitive(world, {.entity = ecs_id(Elevation), .kind = EcsI32});
}
