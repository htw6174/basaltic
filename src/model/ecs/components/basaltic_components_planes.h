#ifndef BASALTIC_COMPONENTS_PLANES_H_INCLUDED
#define BASALTIC_COMPONENTS_PLANES_H_INCLUDED

#include "flecs.h"
#include "htw_geomap.h"
#include "htw_random.h"
#include "khash.h"

#undef ECS_META_IMPL
#undef BC_DECL
#ifndef BC_COMPONENT_IMPL
#define ECS_META_IMPL EXTERN
#define BC_DECL extern
#else
#define BC_DECL
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

// Not typically used as a component, just need reflection data about this struct. Might be useful for brushes though
ECS_STRUCT(CellData, {
    s16 height; // Each height step represents 100m of elevation change TODO: only need to represent ~200 values here, could change to s8. Could probably reduce range of other values too
    u16 geology;
    s16 groundwater; // If > 0: Units undefined atm, base decrease of 1/hour; if <= 0: Represents number of hours without groundwater, always decreases by 1/hour 
    u16 surfacewater;
    u32 understory;
    u32 canopy;
    u16 humidityPreference;
    u16 visibility; // bitmask
});

ECS_STRUCT(Plane, {
    htw_ChunkMap *chunkMap;
});

ECS_ENUM(HexDirection, {
    HEX_DIR_NORTH_EAST, // 0, 1
    HEX_DIR_EAST, // 1, 0
    HEX_DIR_SOUTH_EAST, // 1, -1
    HEX_DIR_SOUTH_WEST, // 0, -1
    HEX_DIR_WEST, // -1, 0
    HEX_DIR_NORTH_WEST, // -1, 1
    HEX_DIR_COUNT // Limit for iterating over values in this enum
});

typedef htw_geo_GridCoord Position, Destination;

BC_DECL ECS_COMPONENT_DECLARE(Position);
BC_DECL ECS_COMPONENT_DECLARE(Destination);
BC_DECL ECS_TAG_DECLARE(IsOn); // Relationship type for entities on a Plane
BC_DECL ECS_TAG_DECLARE(CellRoot); // For marking entities that contain multiple child entities occupying the same cell

void BcPlanesImport(ecs_world_t *world);

// TODO: these may not belong here, but it works for now (implementation depends on this header for the khash decleration)
/// Allows entities with (GridPosition, (IsOn, _)) to be located via hashmap
ecs_entity_t plane_GetRootEntity(ecs_world_t *world, ecs_entity_t plane, Position pos);
void plane_PlaceEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position pos);
void plane_RemoveEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position pos);
void plane_MoveEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position oldPos, Position newPos);

CellData *bc_getCellByIndex(htw_ChunkMap *chunkMap, u32 chunkIndex, u32 cellIndex);

s32 plane_GetCellBiotemperature(const Plane *plane, htw_geo_GridCoord pos);
float plane_CanopyGrowthRate(const Plane *plane, htw_geo_GridCoord pos);

#endif // BASALTIC_COMPONENTS_PLANES_H_INCLUDED
