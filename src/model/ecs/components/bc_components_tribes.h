#ifndef BC_COMPONENTS_TRIBES_H_INCLUDED
#define BC_COMPONENTS_TRIBES_H_INCLUDED

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

ECS_STRUCT(Tribe, {
    s32 influence;
});

ECS_STRUCT(Village, {
    s32 level;
});

// 1 unit of food = amount required by 1 average size actor per day
ECS_STRUCT(Stockpile, {
    s32 grain;
    s32 fruit;
    s32 meat;
    s32 fish;
});

// Relationship where target is a settlement or organization:
// tribe :- Tribe
// village :- (MemberOf, tribe)
// farmer :- (MemberOf, village)
// adventurer :- (MemberOf, tribe)
BC_DECL ECS_TAG_DECLARE(MemberOf);

// relationship to group individuals by age category
BC_DECL ECS_TAG_DECLARE(LifeStage);

void BcTribesImport(ecs_world_t *world);

#endif // BC_COMPONENTS_TRIBES_H_INCLUDED
