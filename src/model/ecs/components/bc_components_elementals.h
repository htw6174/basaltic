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
BC_DECL ECS_TAG_DECLARE(EgoTectonicSpirit);
BC_DECL ECS_TAG_DECLARE(EgoVolcanoSpirit);
BC_DECL ECS_TAG_DECLARE(EgoEarthSpirit);
BC_DECL ECS_TAG_DECLARE(EgoOceanSpirit);
BC_DECL ECS_TAG_DECLARE(EgoStormSpirit);

BC_DECL ECS_TAG_DECLARE(ActionShiftPlates);
BC_DECL ECS_TAG_DECLARE(ActionErupt);

ECS_STRUCT(SpiritPower, {
    s32 maxValue;
    s32 value;
});

/// Used to vary spirit behavior by age; requires CreationTime on the same entity. Effective lifetime is sum of all fields
ECS_STRUCT(SpiritLifetime, {
    s32 youngSteps;
    s32 matureSteps;
    s32 oldSteps;
});

ECS_STRUCT(PlateShiftStrength, {
    float left;
    float right;
    float falloff;
});

typedef float AngleRadians;
BC_DECL ECS_COMPONENT_DECLARE(AngleRadians);

typedef s32 Elevation;
BC_DECL ECS_COMPONENT_DECLARE(Elevation);

void BcElementalsImport(ecs_world_t *world);

#endif // BC_COMPONENTS_ELEMENTALS_H_INCLUDED
