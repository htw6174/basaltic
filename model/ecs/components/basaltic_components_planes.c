#define BASALTIC_PLANES_IMPL

#include "basaltic_components_planes.h"

ECS_COMPONENT_DECLARE(SpatialStorage);
ECS_COMPONENT_DECLARE(Plane);
ECS_COMPONENT_DECLARE(Position);
ECS_COMPONENT_DECLARE(Destination);
ECS_TAG_DECLARE(IsOn); // Relationship type for entities on a Plane
ECS_TAG_DECLARE(CellRoot); // For marking entities that contain multiple child entities occupying the same cell

void BasalticComponentsPlanesImport(ecs_world_t *world) {
    ECS_MODULE(world, BasalticComponentsPlanes);

    ECS_META_COMPONENT(world, SpatialStorage);
    ECS_META_COMPONENT(world, Plane);
    ECS_COMPONENT_DEFINE(world, Position);
    ECS_COMPONENT_DEFINE(world, Destination);
    ECS_TAG_DEFINE(world, IsOn);
    ecs_add_id(world, IsOn, EcsExclusive);
    ECS_TAG_DEFINE(world, CellRoot);

    ecs_struct(world, {
        .entity = ecs_id(Position),
        .members = {
            { .name = "x", .type = ecs_id(ecs_u32_t) },
            { .name = "y", .type = ecs_id(ecs_u32_t) }
        }
    });

    ecs_struct(world, {
        .entity = ecs_id(Destination),
        .members = {
            { .name = "x", .type = ecs_id(ecs_u32_t) },
            { .name = "y", .type = ecs_id(ecs_u32_t) }
        }
    });

    khash_t(WorldMap) *gm = kh_init(WorldMap);
    kh_resize(WorldMap, gm, HASH_MAP_DEFAULT_SIZE);
    ecs_singleton_set(world, SpatialStorage, {gm});

    // TEST
    // ecs_add_id(world, IsOn, EcsOneOf);
    // Don't do this with the plane component, instead add all planes as children of IsOn
    // ecs_add_pair(world, ecs_id(Plane), EcsChildOf, IsOn);
}

ecs_entity_t plane_GetRootEntity(ecs_world_t *world, ecs_entity_t plane, Position pos) {
    khash_t(WorldMap) *wm = ecs_singleton_get(world, SpatialStorage)->hashMap;
    bc_WorldPosition wp = {.plane = ecs_entity_t_lo(plane), .gridPos = pos};
    khint_t i = kh_get(WorldMap, wm, wp);
    if (i == kh_end(wm)) {
        return 0;
    } else {
        return kh_val(wm, i);
    }
}

void plane_PlaceEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position pos) {
    khash_t(WorldMap) *wm = ecs_singleton_get(world, SpatialStorage)->hashMap;
    bc_WorldPosition wp = {.plane = ecs_entity_t_lo(plane), .gridPos = pos};
    khint_t i = kh_get(WorldMap, wm, wp);
    if (i == kh_end(wm)) {
        // No entry here yet, place directly
        int absent;
        i = kh_put(WorldMap, wm, wp, &absent);
        kh_val(wm, i) = e;
    } else {
        // Need to create a new root entity if valid entity is here already
        ecs_entity_t existing = kh_val(wm, i);
        if (ecs_is_valid(world, existing)) {
            if (ecs_has(world, existing, CellRoot)) {
                // Add as new child
                ecs_add_pair(world, e, EcsChildOf, existing);
            } else {
                // Make new cell root, add both as child
                // FIXME: if 2 entities move onto the same cell in one update, 2 cellroots get created
                ecs_defer_suspend(world);
                ecs_entity_t newRoot = ecs_new(world, CellRoot);
                ecs_add_id(world, newRoot, ecs_childof(plane));
                ecs_defer_resume(world);
                ecs_add_pair(world, existing, EcsChildOf, newRoot);
                ecs_add_pair(world, e, EcsChildOf, newRoot);
                int absent;
                khint_t k = kh_put(WorldMap, wm, wp, &absent);
                kh_val(wm, k) = newRoot;
            }
        } else {
            // Invalid entity here, place directly
            kh_val(wm, i) = e;
        }
    }
}

void plane_RemoveEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position pos) {
    khash_t(WorldMap) *wm = ecs_singleton_get(world, SpatialStorage)->hashMap;
    bc_WorldPosition wp = {.plane = ecs_entity_t_lo(plane), .gridPos = pos};
    khint_t i = kh_get(WorldMap, wm, wp);
    if (i == kh_end(wm)) {
        // entity hasn't been placed on the map yet
    } else {
        ecs_entity_t root = kh_val(wm, i);
        if (e == root) { // TODO: is this the best way to compare IDs?
            // e is cell root, there will be no other entities in the cell after moving
            kh_del(WorldMap, wm, i);
        } else {
            // Restore plane as parent
            ecs_add_pair(world, e, EcsChildOf, plane);
        }
    }
}

void plane_MoveEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position oldPos, Position newPos) {
    plane_RemoveEntity(world, plane, e, oldPos);
    plane_PlaceEntity(world, plane, e, newPos);
}
