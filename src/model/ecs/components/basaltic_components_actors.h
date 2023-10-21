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

/// Relationship where target is either an Ego entity (AI system) or a player; controls what the actor will do in the Planning phase
BC_DECL ECS_TAG_DECLARE(Ego);
/// Default ego that disables all AI behavior; should be applied to player controlled actors
BC_DECL ECS_TAG_DECLARE(EgoNone);
/// Example ego to show setup; wanders around in a random direction
BC_DECL ECS_TAG_DECLARE(EgoWanderer);

/// Relationship where target is an Action; controls what the actor will do in the Execute phase
BC_DECL ECS_TAG_DECLARE(Action);
/// Default action when nothing else is chosen; behavior can depend on entity type e.g. resting, foraging, on lookout, etc.
BC_DECL ECS_TAG_DECLARE(ActionIdle);
BC_DECL ECS_TAG_DECLARE(ActionMove);

/// Relationship where target is another actor; will typically follow the leader's actions instead of acting indpendently
BC_DECL ECS_TAG_DECLARE(FollowerOf);

// TODO: is this seperation really necessary? in most cases group.count == 1 would be the same as individual
/// Tag for actors that represent a single individual
BC_DECL ECS_TAG_DECLARE(Individual);
/// Used for actors that represent a generic group; exclusive with the Individual tag TODO find a way to enforce this
ECS_STRUCT(Group, {
    u32 count;
});

ECS_STRUCT(GrowthRate, {
    u32 stepsRequired;
    u32 progress;
});

/// Simulation step when an entity was created, used to calculate age
typedef u64 CreationTime;
BC_DECL ECS_COMPONENT_DECLARE(CreationTime);

/// Used to spawn batches of prefabs; can add (Randomize[Int/Float/???], meta_field) pairs to the same entity to randomize fields on instanced entities
/// Currently, will automatically place on plane, randomize Position, add Destination, and set CreationTime
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
