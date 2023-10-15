#ifndef BASALTIC_COMPONENTS_ACTORS_H_INCLUDED
#define BASALTIC_COMPONENTS_ACTORS_H_INCLUDED

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

BC_DECL ECS_TAG_DECLARE(Ego); // Relationship where target is a 'controller': either an AI system or a player
BC_DECL ECS_TAG_DECLARE(EgoNone); // Default ego that disables all AI behavior; should be applied to player controlled actors
BC_DECL ECS_TAG_DECLARE(EgoWanderer); // Example ego to show setup; wanders around in a random direction

BC_DECL ECS_TAG_DECLARE(Action);
BC_DECL ECS_TAG_DECLARE(ActionIdle);
BC_DECL ECS_TAG_DECLARE(ActionMove);

BC_DECL ECS_TAG_DECLARE(FollowerOf); // Relationship where target is another actor; will typically follow the leader's actions instead of acting indpendently

// TODO: is this seperation really necessary? in most cases group.count == 1 would be the same as individual
BC_DECL ECS_TAG_DECLARE(Individual); // Tag for actors that represent a single individual
// Used for actors that represent a generic group; exclusive with the Individual tag TODO find a way to enforce this
ECS_STRUCT(Group, {
    u32 count;
});

ECS_STRUCT(GrowthRate, {
    u32 stepsRequired;
    u32 progress;
});

ECS_STRUCT(Spawner, {
    ecs_entity_t prefab;
    u32 count;
    bool oneShot;
});

ECS_ENUM(ActorSize, {
    ACTOR_SIZE_NONE = 0, // incorporial things
    ACTOR_SIZE_MINISCULE = 1, // rats, insects, characters this size are usually treated as swarms
    ACTOR_SIZE_TINY = 2, // human baby, snake, rabbit
    ACTOR_SIZE_SMALL = 3, // human child, medium dog
    ACTOR_SIZE_AVERAGE = 4, // human adult, wolf
    ACTOR_SIZE_LARGE = 5, // horse, crocodile
    ACTOR_SIZE_HUGE = 6, // moose, rhino
    ACTOR_SIZE_ENORMOUS = 7, //elephant and larger
});

ECS_STRUCT(Condition, {
    s16 maxHealth;
    s16 health;
    s16 maxStamina;
    s16 stamina;
});

void BcActorsImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_ACTORS_H_INCLUDED
