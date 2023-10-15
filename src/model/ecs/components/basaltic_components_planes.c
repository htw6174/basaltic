#define BASALTIC_PLANES_IMPL
#include "basaltic_components_planes.h"
#include <math.h>

ECS_COMPONENT_DECLARE(SpatialStorage);
ECS_COMPONENT_DECLARE(Plane);
ECS_COMPONENT_DECLARE(Position);
ECS_COMPONENT_DECLARE(Destination);
ECS_TAG_DECLARE(IsOn); // Relationship type for entities on a Plane
ECS_TAG_DECLARE(CellRoot); // For marking entities that contain multiple child entities occupying the same cell

void BcPlanesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcPlanes);

    ECS_META_COMPONENT(world, SpatialStorage);
    ECS_META_COMPONENT(world, CellData);
    ECS_META_COMPONENT(world, Plane);
    ECS_COMPONENT_DEFINE(world, Position);
    ECS_COMPONENT_DEFINE(world, Destination);
    ECS_TAG_DEFINE(world, IsOn);
    ecs_add_id(world, IsOn, EcsExclusive);
    ecs_add_id(world, IsOn, EcsTraversable);
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

CellData *bc_getCellByIndex(htw_ChunkMap *chunkMap, u32 chunkIndex, u32 cellIndex) {
    CellData *cell = chunkMap->chunks[chunkIndex].cellData;
    return &cell[cellIndex];
}

/**
 * @brief Represents mean annual temperature at a cell on the plane, determined by distance to the plane origin and cell height. Approximate range from -30c to +30c
 *
 * @param plane p_plane:...
 * @param pos p_pos:...
 * @return Biotemperature in centicelsius (degrees celsius * 100)
 */
s32 plane_GetCellBiotemperature(const Plane *plane, htw_geo_GridCoord pos) {
    // "Temperatures in the atmosphere decrease with height at an average rate of 6.5 °C per kilometer"; height steps are 100m
    const s32 centicelsiusPerHeightUnit = -65;
    s32 latitudeTemp = htw_geo_circularGradientByGridCoord(
        plane->chunkMap, pos, (htw_geo_GridCoord){0, 0}, -2000, 4000, plane->chunkMap->mapWidth * 0.67);
    s32 altitude = ((CellData*)htw_geo_getCell(plane->chunkMap, pos))->height; // meters * 100
    return latitudeTemp + (abs(altitude) * centicelsiusPerHeightUnit);
}

s32 plane_GetCellTemperature(const Plane *plane, htw_geo_GridCoord pos) {
    // TODO: keep track of current season / temp variation at the plane level?
    return plane_GetCellBiotemperature(plane, pos);
}

/**
 * @brief Growth rate dependent on understory coverage % and canopy coverage %; low at the extremes and high in the middle
 *
 * @param plane p_plane:...
 * @param pos p_pos:...
 * @return float
 */
float plane_CanopyGrowthRate(const Plane *plane, htw_geo_GridCoord pos) {
    CellData *cell = htw_geo_getCell(plane->chunkMap, pos);
    // % of max coverage
    float understoryCoverage = (float)cell->understory / 255.0;
    float canopyCoverage = (float)cell->canopy / 255.0;
    // 5% at 0% canopy, maxes out at 100% at 95% canopy
    float shrubRatio = fmaxf(canopyCoverage + 0.05, 1.0);
    // decreasing benefit of high understory coverage as canopy takes over
    float maxUnderstoryCoverage = 1.0 - canopyCoverage;
    return shrubRatio * understoryCoverage * maxUnderstoryCoverage;
}
