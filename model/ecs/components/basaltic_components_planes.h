#ifndef BASALTIC_COMPONENTS_PLANES_H_INCLUDED
#define BASALTIC_COMPONENTS_PLANES_H_INCLUDED

#include "flecs.h"
#include "htw_geomap.h"
#include "htw_random.h"
#include "khash.h"

#undef ECS_META_IMPL
#ifndef BASALTIC_PLANES_IMPL
#define ECS_META_IMPL EXTERN // Ensure meta symbols are only defined once
#endif

#define HASH_MAP_DEFAULT_SIZE (1 << 16)

typedef struct {
    // TO CONSIDER: implications of using only the 'raw' entity id, without the high bits?
    u32 plane;
    htw_geo_GridCoord gridPos;
} bc_WorldPosition;

#define bc_worldPosition_hash_func(key) xxh_hash2d(key.plane, key.gridPos.x, key.gridPos.y)
#define bc_worldPosition_hash_equal(a, b) (a.plane == b.plane && htw_geo_isEqualGridCoords(a.gridPos, b.gridPos))

KHASH_INIT(WorldMap, bc_WorldPosition, ecs_entity_t, 1, bc_worldPosition_hash_func, bc_worldPosition_hash_equal)

// Singleton
ECS_STRUCT(SpatialStorage, {
    khash_t(WorldMap) *hashMap;
});

ECS_STRUCT(Plane, {
    htw_ChunkMap *chunkMap;
});

typedef htw_geo_GridCoord Position, Destination;

extern ECS_COMPONENT_DECLARE(Position);
extern ECS_COMPONENT_DECLARE(Destination);
extern ECS_TAG_DECLARE(IsOn); // Relationship type for entities on a Plane
extern ECS_TAG_DECLARE(CellRoot); // For marking entities that contain multiple child entities occupying the same cell

void BcPlanesImport(ecs_world_t *world);

// TODO: these may not belong here, but it works for now (implementation depends on this header for the khash decleration)
// Allows entities with (GridPosition, (IsOn, _)) to be located via hashmap
ecs_entity_t plane_GetRootEntity(ecs_world_t *world, ecs_entity_t plane, Position pos);
void plane_PlaceEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position pos);
void plane_RemoveEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position pos);
void plane_MoveEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position oldPos, Position newPos);

#endif // BASALTIC_COMPONENTS_PLANES_H_INCLUDED
