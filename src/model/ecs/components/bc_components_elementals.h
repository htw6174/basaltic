#ifndef BC_COMPONENTS_ELEMENTALS_H_INCLUDED
#define BC_COMPONENTS_ELEMENTALS_H_INCLUDED

#include "htw_core.h"
#include "flecs.h"

#undef ECS_META_IMPL
#undef BC_DECL
#ifndef BC_COMPONENT_IMPL
#define ECS_META_IMPL EXTERN
#define BC_DECL extern
#else
#define BC_DECL
#endif

BC_DECL ECS_TAG_DECLARE(ElementalSpirit);
BC_DECL ECS_TAG_DECLARE(EgoStormSpirit);
BC_DECL ECS_TAG_DECLARE(EgoEarthSpirit);
BC_DECL ECS_TAG_DECLARE(EgoOceanSpirit);
BC_DECL ECS_TAG_DECLARE(EgoVolcanoSpirit);

BC_DECL ECS_TAG_DECLARE(ActionErupt);

ECS_STRUCT(SpiritPower, {
    s32 maxValue;
    s32 value;
});

void BcElementalsImport(ecs_world_t *world);

#endif // BC_COMPONENTS_ELEMENTALS_H_INCLUDED
