#define BC_COMPONENT_IMPL
#include "basaltic_components_planes.h"
#include <math.h>

void BcPlanesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcPlanes);

    ECS_META_COMPONENT(world, SpatialStorage);

    ECS_COMPONENT_DEFINE(world, CellWaterways);
    ecs_struct(world, {
        .entity = ecs_id(CellWaterways),
        .members = {
            { .name = "opaque", .type = ecs_id(ecs_u32_t) }
        }
    });

    ECS_META_COMPONENT(world, RiverSegment);

    ECS_META_COMPONENT(world, CellData);
    { // Add range to CellData members
        ecs_entity_t e;
        e = ecs_lookup_child(world, ecs_id(CellData), "height");
        ecs_set(world, e, EcsMemberRanges, {.value = {INT8_MIN, INT8_MAX}});
        e = ecs_lookup_child(world, ecs_id(CellData), "visibility");
        ecs_set(world, e, EcsMemberRanges, {.value = {0, 2}});
        e = ecs_lookup_child(world, ecs_id(CellData), "geology");
        ecs_set(world, e, EcsMemberRanges, {.value = {0, UINT16_MAX}});
        e = ecs_lookup_child(world, ecs_id(CellData), "tracks");
        ecs_set(world, e, EcsMemberRanges, {.value = {0, UINT16_MAX}});
        e = ecs_lookup_child(world, ecs_id(CellData), "groundwater");
        ecs_set(world, e, EcsMemberRanges, {.value = {INT16_MIN, INT16_MAX}});
        e = ecs_lookup_child(world, ecs_id(CellData), "surfacewater");
        ecs_set(world, e, EcsMemberRanges, {.value = {0, UINT16_MAX}});
        e = ecs_lookup_child(world, ecs_id(CellData), "humidityPreference");
        ecs_set(world, e, EcsMemberRanges, {.value = {0, UINT16_MAX}});
        e = ecs_lookup_child(world, ecs_id(CellData), "waterways");
        ecs_set(world, e, EcsMemberRanges, {.value = {0, UINT32_MAX}});
        e = ecs_lookup_child(world, ecs_id(CellData), "understory");
        ecs_set(world, e, EcsMemberRanges, {.value = {0, UINT32_MAX}});
        e = ecs_lookup_child(world, ecs_id(CellData), "canopy");
        ecs_set(world, e, EcsMemberRanges, {.value = {0, UINT32_MAX}});
    }

    ECS_META_COMPONENT(world, ClimateType);
    ECS_META_COMPONENT(world, Season);
    ECS_META_COMPONENT(world, Climate);
    ECS_META_COMPONENT(world, Plane);

    ECS_COMPONENT_DEFINE(world, HexDirection);
    ecs_enum(world, {
        .entity = ecs_id(HexDirection),
        .constants = {
            {.name = "HEX_DIRECTION_NORTH_EAST", .value = 0},
            {.name = "HEX_DIRECTION_EAST"},
            {.name = "HEX_DIRECTION_SOUTH_EAST"},
            {.name = "HEX_DIRECTION_SOUTH_WEST"},
            {.name = "HEX_DIRECTION_WEST"},
            {.name = "HEX_DIRECTION_NORTH_WEST"},
        }
    });

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
    // TODO: the spatial storage should only be accessed from the corresponding get/set methods, and only one is needed per model. Should be a private static var instead of an ECS singleton?
    ecs_singleton_set(world, SpatialStorage, {gm});

    // TEST
    // ecs_add_id(world, IsOn, EcsOneOf);
    // Don't do this with the plane component, instead add all planes as children of IsOn
    // ecs_add_pair(world, ecs_id(Plane), EcsChildOf, IsOn);
}

// FIXME: this shouldn't return false for new entities created this step
bool plane_IsValidRootEntity(ecs_world_t *world, ecs_entity_t root, ecs_entity_t plane, Position pos) {
    // Ensure root is still valid and has correct world position. If not, should clear hashmap entry
    if (ecs_is_valid(world, root)) {
        if (ecs_has(world, root, CellRoot)) return true;
        const Position *rootPos = ecs_get(world, root, Position);
        ecs_entity_t rootPlane = ecs_get_target(world, root, IsOn, 0);
        if (rootPos && rootPlane == plane) {
            if (htw_geo_isEqualGridCoords(*rootPos, pos)) {
                return true;
            }
        }
    }
    return false;
}

void plane_SetupCellRoot(ecs_world_t *world, ecs_entity_t newRoot, ecs_entity_t plane, Position pos) {
    ecs_add(world, newRoot, CellRoot);
    ecs_add_id(world, newRoot, ecs_childof(plane));
    //ecs_add_pair(world, newRoot, IsOn, plane);
    //ecs_set(world, newRoot, Position, {pos.x, pos.y});
}

ecs_entity_t plane_GetRootEntity(ecs_world_t *world, ecs_entity_t plane, Position pos) {
    khash_t(WorldMap) *wm = ecs_singleton_get(world, SpatialStorage)->hashMap;
    bc_WorldPosition wp = {.plane = ecs_entity_t_lo(plane), .gridPos = pos};
    khint_t i = kh_get(WorldMap, wm, wp);
    if (i == kh_end(wm)) {
        return 0;
    } else {
        ecs_entity_t root = kh_val(wm, i);
        // current value here may be an entity that previously moved. If so, clear the key for this world position
        if (plane_IsValidRootEntity(world, root, plane, pos)) {
            return root;
        } else {
            kh_del(WorldMap, wm, i);
            return 0;
        }
    }
}

void plane_PlaceEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position pos) {
    ecs_assert(ecs_is_valid(world, plane), 0, "Plane not valid!");
    //plane = ecs_entity_t_lo(plane);
    khash_t(WorldMap) *wm = ecs_singleton_get(world, SpatialStorage)->hashMap;
    bc_WorldPosition wp = {.plane = plane, .gridPos = pos};
    khint_t i = kh_get(WorldMap, wm, wp);
    if (i == kh_end(wm)) {
        // No entry here yet, add key and set value to place here
        int absent;
        i = kh_put(WorldMap, wm, wp, &absent);
        kh_val(wm, i) = e;
    } else {
        // Need to create a new root entity if valid entity is here already
        ecs_entity_t root = kh_val(wm, i);
        if (plane_IsValidRootEntity(world, root, plane, pos)) {
            if (ecs_has(world, root, CellRoot)) {
                // Already a root entity here, add as new child
                ecs_add_pair(world, e, EcsChildOf, root);
            } else {
                // Make new cell root, add both as child
                ecs_entity_t newRoot = ecs_new_id(world);
                // Must set hashmap value to new entity before doing setup, so that OnSet observers don't loop here forever
                //int absent;
                //khint_t k = kh_put(WorldMap, wm, wp, &absent);
                kh_val(wm, i) = newRoot;
                if (ecs_is_deferred(world)) {
                    ecs_defer_suspend(world);
                    plane_SetupCellRoot(world, newRoot, plane, pos);
                    ecs_defer_resume(world);
                } else {
                    plane_SetupCellRoot(world, newRoot, plane, pos);
                }
                ecs_add_pair(world, root, EcsChildOf, newRoot);
                ecs_add_pair(world, e, EcsChildOf, newRoot);
            }
        } else {
            // Invalid entity here, make child of plane and set value to place here / make this cell's root
            ecs_add_pair(world, e, EcsChildOf, plane);
            kh_val(wm, i) = e;
        }
    }
}

void plane_RemoveEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position pos) {
    khash_t(WorldMap) *wm = ecs_singleton_get(world, SpatialStorage)->hashMap;
    bc_WorldPosition wp = {.plane = ecs_entity_t_lo(plane), .gridPos = pos};
    khint_t i = kh_get(WorldMap, wm, wp);
    if (i == kh_end(wm)) {
        // entity hasn't been placed on the map yet, nothing to remove
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

// Doesn't need to know where the entity moved *from*, because at lookup time we can check if the current entry should be there or not
void plane_MoveEntity(ecs_world_t *world, ecs_entity_t plane, ecs_entity_t e, Position newPos)
{
    plane_PlaceEntity(world, plane, e, newPos);
}

CellData *bc_getCellByIndex(htw_ChunkMap *chunkMap, u32 chunkIndex, u32 cellIndex) {
    CellData *cell = chunkMap->chunks[chunkIndex].cellData;
    return &cell[cellIndex];
}

s32 plane_GetCellTemperature(const Plane *plane, const Climate *climate, htw_geo_GridCoord pos) {
    if (climate == NULL) {
        return 2000; // Neutral ambient temperature if climate is undefined
    }
    s32 biotemp;
    switch(climate->type) {
        case CLIMATE_TYPE_UNIFORM:
            biotemp = climate->equatorBiotemp;
            break;
        case CLIMATE_TYPE_RADIAL:
            biotemp = htw_geo_circularGradientByGridCoord(
                plane->chunkMap,
                pos,
                (htw_geo_GridCoord){0, 0},
                climate->poleBiotemp,
                climate->equatorBiotemp,
                plane->chunkMap->mapHeight * 0.57735 // = outer radius of equilateral triangle = edge / sqrt(3)
            );
            break;
        case CLIMATE_TYPE_BANDS:
            // TODO
        default:
            ecs_err("Unhandled Climate Type: %u", climate->type);
            biotemp = 2000;
            break;
    }

    s32 elevation = ((CellData*)htw_geo_getCell(plane->chunkMap, pos))->height; // meters * 100
    return biotemp + climate->season.temperatureModifier + (abs(elevation) * climate->tempChangePerElevationStep);
}

s32 plane_GetCellBiotemperature(const Plane *plane, htw_geo_GridCoord pos) {
    // "Temperatures in the atmosphere decrease with height at an average rate of 6.5 °C per kilometer"; height steps are 100m
    const s32 centicelsiusPerHeightUnit = -65;
    s32 latitudeTemp = htw_geo_circularGradientByGridCoord(
        plane->chunkMap, pos, (htw_geo_GridCoord){0, 0}, -2000, 4000, plane->chunkMap->mapHeight * 0.57735);
    s32 altitude = ((CellData*)htw_geo_getCell(plane->chunkMap, pos))->height; // meters * 100
    return latitudeTemp + (abs(altitude) * centicelsiusPerHeightUnit);
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
    float understoryCoverage = (float)cell->understory / (float)UINT32_MAX;
    float canopyCoverage = (float)cell->canopy / (float)UINT32_MAX;
    // 5% at 0% canopy, maxes out at 100% at 95% canopy
    float shrubRatio = fmaxf(canopyCoverage + 0.05, 1.0);
    // decreasing benefit of high understory coverage as canopy takes over
    float maxUnderstoryCoverage = 1.0 - canopyCoverage;
    return shrubRatio * understoryCoverage * maxUnderstoryCoverage;
}

void extractCellWaterway(const htw_ChunkMap *cm, htw_geo_GridCoord position, HexDirection referenceDirection, const CellData *cell, CellWaterConnections *connections) {
    u32 ww = *(u32*)&(cell->waterways);
    for (int i = 0; i < HEX_DIRECTION_COUNT; i++) {
        HexDirection dir = (referenceDirection + i) % HEX_DIRECTION_COUNT;
        u32 shift = dir * 5;
        u32 edge = ww >> shift;
        u32 segmentMask = (1 << 2) - 1;
        u32 connectionMask = 1 << 4;
        u32 segment = edge & segmentMask;
        u32 connection = edge & connectionMask;

        connections->segments[i] = segment != 0;
        connections->connectionsOut[i] = connection != 0;

        // Get connection data from neighbor cell in direction, in side opposite direction
        connections->neighbors[i] = htw_geo_getCell(cm, POSITION_IN_DIRECTION(position, dir));
        u32 wn = *(u32*)&(connections->neighbors[i]->waterways);
        dir = htw_geo_hexDirectionOpposite(dir);
        shift = dir * 5;
        edge = wn >> shift;
        connection = edge & connectionMask;
        connections->connectionsIn[i] = connection != 0;
    }
}

void storeCellWaterway(htw_ChunkMap *cm, HexDirection referenceDirection, CellData *cell, const CellWaterConnections *connections) {
    u32 ww = 0; // new blank CellWaterways
    for (int i = 0; i < HEX_DIRECTION_COUNT; i++) {
        HexDirection dir = (referenceDirection + i) % HEX_DIRECTION_COUNT;
        u32 shift = dir * 5;
        u32 connectionShift = 4;
        u32 segment = connections->segments[i];
        u32 connection = connections->connectionsOut[i];
        u32 edge = segment | (connection << connectionShift);
        edge = edge << shift;
        // merge in edge
        ww |= edge;
    }
    cell->waterways = *(CellWaterways*)&ww;
}

CellWaterConnections plane_extractCellWaterways(const htw_ChunkMap *cm, htw_geo_GridCoord position) {
    CellWaterConnections cw;
    CellData *cell = htw_geo_getCell(cm, position);
    extractCellWaterway(cm, position, 0, cell, &cw);
    return cw;
}

RiverConnection plane_riverConnectionFromCells(const htw_ChunkMap *cm, htw_geo_GridCoord a, htw_geo_GridCoord b) {
    RiverConnection rc;
    CellData *c1 = htw_geo_getCell(cm, a);
    CellData *c2 = htw_geo_getCell(cm, b);

    // for convience, ensure a is coord of uphill cell
    if (c2->height > c1->height) {
        rc.uphillCell = c2;
        rc.downhillCell = c1;
        htw_geo_GridCoord temp = a;
        a = b;
        b = temp;
    } else {
        rc.uphillCell = c1;
        rc.downhillCell = c2;
    }
    rc.upToDownDirection = htw_geo_relativeHexDirection(a, b);

    // extract info for uphill cell
    extractCellWaterway(cm, a, rc.upToDownDirection, rc.uphillCell, &rc.uphill);
    // for downhill
    extractCellWaterway(cm, b, htw_geo_hexDirectionOpposite(rc.upToDownDirection), rc.downhillCell, &rc.downhill);

    return rc;
}

void plane_applyRiverConnection(htw_ChunkMap *cm, const RiverConnection *rc) {
    storeCellWaterway(cm, rc->upToDownDirection, rc->uphillCell, &rc->uphill);
    storeCellWaterway(cm, htw_geo_hexDirectionOpposite(rc->upToDownDirection), rc->downhillCell, &rc->downhill);
}








