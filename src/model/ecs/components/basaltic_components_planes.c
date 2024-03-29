#include "bc_flecs_utils.h"
#define BC_COMPONENT_IMPL
#include "basaltic_components_planes.h"
#include <math.h>

int serializeCellGeology(const ecs_serializer_t *ser, const void *src) {
    // TODO
    return 0;
}

void BcPlanesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcPlanes);

    ECS_IMPORT(world, FlecsUnits);

    ECS_META_COMPONENT(world, SpatialStorage);

    // Units used for cell data values
    ecs_entity_t DecaMeters = ecs_set_name(world, 0, "DecaMeters");
    ecs_unit(world, {
        .entity = DecaMeters,
        .base = EcsMeters,
        .prefix = EcsDeca,
    });

    ecs_entity_t Volume = ecs_quantity(world, {
        .name = "Volume"
    });

    ecs_entity_t Litres = ecs_set_name(world, 0, "Litres");
    ecs_unit(world, {
        .entity = Litres,
        .quantity = Volume,
        .symbol = "L"
    });

    ecs_entity_t MegaLitres = ecs_set_name(world, 0, "MegaLitres");
    ecs_unit(world, {
        .entity = MegaLitres,
        .base = Litres,
        .prefix = EcsMega
    });

    ecs_entity_t Biomass = ecs_set_name(world, 0, "Biomass");
    ecs_unit(world, {
        .entity = Biomass,
        .base = EcsKiloGrams
    });

    ECS_COMPONENT_DEFINE(world, CellGeology);
    ecs_opaque(world, {
        .entity = ecs_id(CellGeology),
        .type = {
            .as_type = ecs_id(ecs_u16_t),
            .serialize = serializeCellGeology,
            // TODO: figure out details of using opaque types
        }
    });

    ECS_COMPONENT_DEFINE(world, CellWaterways);
    ecs_struct(world, {
        .entity = ecs_id(CellWaterways),
        .members = {
            { .name = "opaque", .type = ecs_id(ecs_u32_t) }
        }
    });

    ECS_COMPONENT_DEFINE(world, CellData);
    ecs_struct(world, {
        .entity = ecs_id(CellData),
        .members = {
            {
                .name = "height",
                .type = ecs_id(ecs_i8_t),
                .unit = DecaMeters,
                .range = {.min = INT8_MIN, .max = INT8_MAX}
            },
            {
                .name = "visibility",
                .type = ecs_id(ecs_u8_t),
                .range = {.min = 0, .max = 2}
            },
            {
                .name = "geology",
                .type = ecs_id(CellGeology)
            },
            {
                .name = "tracks",
               .type = ecs_id(ecs_u16_t),
               .range = {.min = 0, .max = UINT16_MAX}
            },
            {
                .name = "groundwater",
               .type = ecs_id(ecs_i16_t),
               .unit = MegaLitres,
               .range = {.min = INT16_MIN, .max = INT16_MAX}
            },
            {
                .name = "surfacewater",
               .type = ecs_id(ecs_u16_t),
               .unit = MegaLitres,
               .range = {.min = 0, .max = UINT16_MAX}
            },
            {
                .name = "humidityPreference",
               .type = ecs_id(ecs_u16_t),
               .unit = MegaLitres,
               .range = {.min = 0, .max = UINT16_MAX}
            },
            {
                .name = "waterways",
               .type = ecs_id(CellWaterways)
            },
            {
                .name = "understory",
               .type = ecs_id(ecs_u32_t),
               .unit = Biomass,
               .range = {.min = 0, .max = UINT32_MAX}
            },
            {
                .name = "canopy",
               .type = ecs_id(ecs_u32_t),
               .unit = Biomass,
               .range = {.min = 0, .max = UINT32_MAX}
            },
        }
    });

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
    ECS_TAG_DEFINE(world, IsIn);
    ecs_add_id(world, IsIn, EcsExclusive);
    ecs_add_id(world, IsIn, EcsTransitive);
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

    bc_loadModuleScript(world, "model/plecs/modules");
}

// FIXME: this shouldn't return false for new entities created this step
bool plane_IsValidRootEntity(ecs_world_t *world, ecs_entity_t root, ecs_entity_t plane, Position pos) {
    // Ensure root is still valid and has correct world position. If not, should clear hashmap entry
    if (ecs_is_valid(world, root)) {
        if (ecs_has(world, root, CellRoot)) return true;
        const Position *rootPos = ecs_get(world, root, Position);
        ecs_entity_t rootPlane = ecs_get_target(world, root, IsIn, 0);
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
    ecs_add_pair(world, newRoot, IsIn, plane);
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
        // No entry here yet: add key, set value to place here, and place in plane
        int absent;
        i = kh_put(WorldMap, wm, wp, &absent);
        kh_val(wm, i) = e;
        ecs_add_pair(world, e, IsIn, plane);
    } else {
        // Need to create a new root entity if valid entity is here already
        ecs_entity_t root = kh_val(wm, i);
        if (plane_IsValidRootEntity(world, root, plane, pos)) {
            if (ecs_has(world, root, CellRoot)) {
                // Already a root entity here, place 'in' cellRoot
                ecs_add_pair(world, e, IsIn, root);
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
                ecs_add_pair(world, root, IsIn, newRoot);
                ecs_add_pair(world, e, IsIn, newRoot);
            }
        } else {
            // Invalid entity here, place in plane and set value to place here / make this cell's root
            kh_val(wm, i) = e;
            ecs_add_pair(world, e, IsIn, plane);
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
            // Restore plane as container
            ecs_add_pair(world, e, IsIn, plane);
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

s32 plane_GetCellBiotemperature(const Plane *plane, const Climate *climate, htw_geo_GridCoord pos) {
    s32 latitudeTemp;
    switch(climate->type) {
        case CLIMATE_TYPE_UNIFORM:
            latitudeTemp = climate->equatorBiotemp;
            break;
        case CLIMATE_TYPE_RADIAL:
            latitudeTemp = htw_geo_circularGradientByGridCoord(
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
            latitudeTemp = 2000;
            break;
    }
    s32 elevation = ((CellData*)htw_geo_getCell(plane->chunkMap, pos))->height; // meters * 100
    s32 altitudeTemp = (abs(elevation) * climate->tempChangePerElevationStep);
    return latitudeTemp + altitudeTemp;
}

s32 plane_GetCellTemperature(const Plane *plane, const Climate *climate, htw_geo_GridCoord pos) {
    if (climate == NULL) {
        return 2000; // Neutral ambient temperature if climate is undefined
    }

    s32 biotemp = plane_GetCellBiotemperature(plane, climate, pos);
    s32 varyingTemp = climate->season.temperatureModifier;
    return biotemp + varyingTemp;
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

// x: increasing temperature
// y: increasing humidity
const char *lifezoneNames[8][6] = {
    {"freezing desert", "cold desert", "cold desert", "cool desert", "warm desert", "hot desert"},
    {"freezing desert", "cold desert", "cold desert", "cool desert", "warm desert", "desert scrub"},
    {"freezing desert", "cold desert", "cold desert", "cool desert", "desert scrub", "thorn woodland"},
    {"freezing desert", "cold desert", "cold desert", "desert scrub", "thorn steppe", "parched forest"},
    {"freezing desert", "dry tundra", "dry scrub", "steppe", "dry forest", "dry tropical forest"},
    {"freezing desert", "moist tundra", "moist boreal forest", "moist temperate forest", "moist forest", "moist tropical forest"},
    {"glacier", "wet tundra", "wet boreal forest", "wet temperate forest", "wet forest", "wet tropical forest"},
    {"glacier", "rain tundra", "boreal rain forest", "temperate rain forest", "rain forest", "tropical rain forest"},
};

const char *waterzoneNames[6] = {"arctic ocean", "cold ocean", "cool ocean", "temperate ocean", "warm ocean", "tropical ocean"};

// TODO: lots of ways to make this better/more interesting, can have several different adjective/noun pools based on different cell properties
const char *plane_getCellLifezoneName(const Plane *plane, const Climate *climate, htw_geo_GridCoord pos) {
    CellData *cell = htw_geo_getCell(plane->chunkMap, pos);

    s32 biotemp = plane_GetCellBiotemperature(plane, climate, pos);
    s32 petRatio = cell->humidityPreference;
    // TODO: translate biotemp and petRatio into range of name array
    s32 biotempRemap = sqrt(biotemp); // [0, 2400] -> [0, 48]
    biotempRemap = remap_int(biotempRemap, 2, 48, 1, 5);
    biotemp = biotemp < -1750 ? 0 : biotempRemap;
    if (cell->height < 0) {
        return waterzoneNames[biotemp];
    }

    petRatio = sqrt(petRatio); // [0, 8192] -> [0, 90]
    petRatio = remap_int(petRatio, 0, 90, 0, 7);

    CLAMP(petRatio, 0, 7);
    CLAMP(biotemp, 0, 5);
    return lifezoneNames[petRatio][biotemp];
}








