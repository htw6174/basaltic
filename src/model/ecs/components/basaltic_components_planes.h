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

typedef struct {
    // 0 = no river segment
    unsigned
    riverSizeNE : 3,
    riverSizeE  : 3,
    riverSizeSE : 3,
    riverSizeSW : 3,
    riverSizeW  : 3,
    riverSizeNW : 3;

    // 0 = clockwise around cell, 1 = counter-clockwise around cell
    bool
    riverDirNE  : 1,
    riverDirE   : 1,
    riverDirSE  : 1,
    riverDirSW  : 1,
    riverDirW   : 1,
    riverDirNW  : 1;

    // last 6 bits unused, may be used for on-cell lakes in future
    // could use a few to store type of waterway, e.g. lake, swamp, estuary
    unsigned : 6;
} CellWaterways;

BC_DECL ECS_COMPONENT_DECLARE(CellWaterways);

// Expanded, inspectable form of river segment data usually packed into CellWaterways
ECS_STRUCT(RiverSegment, {
    s32 size; // range [0, 7]
    bool direction; // 0 = clockwise around cell, 1 = counter-clockwise around cell TODO make an enum for this?
});

// Not typically used as a component, just need reflection data about this struct. Might be useful for brushes though
ECS_STRUCT(CellData, {
    s8 height; // Each height step represents 100m of elevation change
    u8 visibility; // bitmask
    u16 geology; // reserving some space for later TODO: how to implement and represent varying cell geology
    u16 tracks; // Increases when actors move into a tile, decreases over time. Persists longer in some terrain types than others
    s16 groundwater; // If > 0: Units currently undefined, base decrease of 1/hour; if <= 0: Represents number of hours without groundwater, always decreases by 1/hour
    u16 surfacewater; // Converts into groundwater and evaporates over time, rate based on geology and vegetation
    u16 humidityPreference; // Type of vegetation growing here; the higher this is, the less time water can be unavailable before vegetation starts dying off. Moves toward average water availability over time
    CellWaterways waterways;
    u32 understory; // Units currently undefined; biomass of grasses, shrubs, etc.
    u32 canopy; // Units currently undefined; biomass of trees
});

ECS_ENUM(ClimateType, {
    // Uses equator temp everywhere
    CLIMATE_TYPE_UNIFORM,
    // Gradient from pole temp at world origin to equator temp at farthest distance from origin
    CLIMATE_TYPE_RADIAL,
    // Gradient from pole temp at y = 0, to equator temp at y = height / 2, to pole temp at y = height
    CLIMATE_TYPE_BANDS
});

// temperature modifer is 0 at start, middle, and end of season cycle, first rising to +range then descending to -range
ECS_STRUCT(Season, {
    s32 temperatureRange; // in centicelsius
    s32 temperatureModifier; // in centicelsius
    u32 cycleLength; // in hours
    u32 cycleProgress; // in hours
});

// Temperature units are in centicelsius
ECS_STRUCT(Climate, {
    s32 poleBiotemp;
    s32 equatorBiotemp;
    // If negative, gets colder farther from sea level (height = 0)
    s32 tempChangePerElevationStep;
    ClimateType type;
    Season season;
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
void plane_MoveEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position newPos);

CellData *bc_getCellByIndex(htw_ChunkMap *chunkMap, u32 chunkIndex, u32 cellIndex);


s32 plane_GetCellTemperature(const Plane *plane, const Climate *climate, htw_geo_GridCoord pos);

/**
 * @brief Represents mean annual temperature at a cell on the plane, determined by distance to the plane origin and cell height. Approximate range from -30c to +30c
 *
 * @param plane p_plane:...
 * @param pos p_pos:...
 * @return Biotemperature in centicelsius (degrees celsius * 100)
 */
//s32 plane_GetCellBiotemperature(const Plane *plane, htw_geo_GridCoord pos);
float plane_CanopyGrowthRate(const Plane *plane, htw_geo_GridCoord pos);

#endif // BASALTIC_COMPONENTS_PLANES_H_INCLUDED
