#include "bc_elementals_systems.h"
#include "basaltic_phases.h"
#include "basaltic_components.h"

HexDirection getDirLeft(HexDirection dir);
HexDirection getDirRight(HexDirection dir);
/// 0 rad == HEX_DIR_NORTH_EAST, continues clockwise
HexDirection radToHexDir(AngleRadians angle);

HexDirection getDirLeft(HexDirection dir) {
    return MOD(dir - 1, HEX_DIR_COUNT);
}

HexDirection getDirRight(HexDirection dir) {
    return (dir + 1) % HEX_DIR_COUNT;
}

HexDirection radToHexDir(AngleRadians angle) {
    // FIXME: respect radian conventions by making 0 rad == East and continuing CCW
    // map [0:TAU] to an int in [0:6]
    int quant = roundf((angle / PI) * 3);
    // remap [0, 6] to [1, -5] (reverse turning direction and offset by 1)
    quant = -quant + 1;
    return (HexDirection)MOD(quant, HEX_DIRECTION_COUNT);
}

htw_geo_GridCoord hexLineEndPoint(htw_geo_GridCoord start, float length, float angleRad) {
    // get cartesian position for hex
    float startX, startY;
    htw_geo_getHexCellPositionSkewed(start, &startX, &startY);
    // get x and y offset of end
    float dX = cosf(angleRad) * length;
    float dY = sinf(angleRad) * length;
    // cartesian coord of end hex
    float endX = startX + dX;
    float endY = startY + dY;
    // hex coord of end
    htw_geo_GridCoord end = htw_geo_cartesianToHexCoord(endX, endY);
    return end;
}

htw_geo_GridCoord hexOnLine(htw_geo_GridCoord a, htw_geo_GridCoord b, s32 dist) {
    u32 endPointDist = htw_geo_hexGridDistance(a, b);
    // lerp factor
    float t = (1.0 / endPointDist) * dist;
    // lerp between hex coords
    float q = lerp(a.x, b.x, t);
    float r = lerp(a.y, b.y, t);
    // convert from fractional hex coords to integral
    return htw_geo_hexFractionalToHexCoord(q, r);
}

void shiftTerrainInLine(htw_ChunkMap *cm, htw_geo_GridCoord start, htw_geo_GridCoord towards, s32 strength) {
    //CellData *startCell = htw_geo_getCell(cm, start);
    //startCell->height += strength;

    //s32 particlesCount = (strength * strength) / 2;
    //for (int i = 0; i < particlesCount; i++) {}
    s32 range = sqrt(4.0 * abs(strength));
    for (int i = 0; i < range; i++) {
        // TODO projecting landslides out as particles
        htw_geo_GridCoord lineCoord = hexOnLine(start, towards, i);
        CellData *cell = htw_geo_getCell(cm, lineCoord);
        float mag = 1.0 - ((float)i / range);
        mag *= mag;
        cell->height += strength * mag;
    }
}

void EgoBehaviorTectonic(ecs_iter_t *it) {
    Position *pos = ecs_field(it, Position, 1);
    AngleRadians *angle = ecs_field(it, AngleRadians, 2);
    Destination *dest = ecs_field(it, Destination, 3);
    Step step = *ecs_singleton_get(it->world, Step);

    for (int i = 0; i < it->count; i++) {
        // alternate between moving and shifting
        if (step % 2) {
            // move in similar direction
            if (htw_randRange(24) == 0) {
                angle[i] += PI/24.0;
            }
            dest[i] = POSITION_IN_DIRECTION(pos[i], radToHexDir(angle[i] + htw_randRange(2)));
            ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
        } else {
            ecs_add_pair(it->world, it->entities[i], Action, ActionShiftPlates);
        }
    }
}

void EgoBehaviorVolcano(ecs_iter_t *it) {
    SpiritPower *sp = ecs_field(it, SpiritPower, 1);

    for (int i = 0; i < it->count; i++) {
        if (sp[i].value >= 100) {
            sp[i].value -= 100;
            ecs_add_pair(it->world, it->entities[i], Action, ActionErupt);
        } else {
            ecs_add_pair(it->world, it->entities[i], Action, ActionIdle);
        }
    }
}

void ExecuteShiftPlates(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    AngleRadians *angle = ecs_field(it, AngleRadians, 2);
    Plane *plane = ecs_field(it, Plane, 3);
    htw_ChunkMap *cm = plane->chunkMap;
    PlateShiftStrength *shiftStrength = ecs_field(it, PlateShiftStrength, 4);
    SpiritLifetime *lifetime = ecs_field(it, SpiritLifetime, 5);
    CreationTime *createTime = ecs_field(it, CreationTime, 6);
    Step step = *ecs_field(it, Step, 7);

    for (int i = 0; i < it->count; i++) {
        // Determine effective strength by age and lifetime
        float strengthFactor;
        u64 age = step - createTime[i];
        SpiritLifetime lt = lifetime[i];
        s32 maturityEnd = lt.youngSteps + lt.matureSteps;
        s32 lifeEnd = maturityEnd + lt.oldSteps;
        if (age < lt.youngSteps) {
            strengthFactor = (float)age / lt.youngSteps;
        } else if (age < maturityEnd) {
            strengthFactor = 1.0;
        } else if (age < lifeEnd) {
            s32 oldProgress = age - maturityEnd;
            strengthFactor = 1.0 - ((float)oldProgress / lt.oldSteps);
        } else {
            // End of life, destroy
            strengthFactor = 0.0;
            ecs_delete(it->world, it->entities[i]);
        }

        //Left side
        htw_geo_GridCoord startLeft = positions[i];
        // FIXME: radius of 100 gives much higher hex distance than expected?
        htw_geo_GridCoord endLeft = hexLineEndPoint(startLeft, 100.0, angle[i] + (PI/2.0));
        shiftTerrainInLine(cm, startLeft, endLeft, shiftStrength[i].left * strengthFactor);

        // Right side (forward turned 60 deg CW)
        htw_geo_GridCoord startRight = POSITION_IN_DIRECTION(positions[i], radToHexDir(angle[i] - (PI/3.0)));
        htw_geo_GridCoord endRight = hexLineEndPoint(startRight, 100.0, angle[i] - (PI/2.0));
        shiftTerrainInLine(cm, startRight, endRight, shiftStrength[i].right * strengthFactor);
    }
}

void ExecuteErupt(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Plane *plane = ecs_field(it, Plane, 2);
    htw_ChunkMap *cm = plane->chunkMap;

    for (int i = 0; i < it->count; i++) {
        CellData *cell = htw_geo_getCell(cm, positions[i]);
        cell->height += 1;
    }
}

void TickSpiritPower(ecs_iter_t *it) {
    SpiritPower *sp = ecs_field(it, SpiritPower, 1);

    for (int i = 0; i < it->count; i++) {
        sp[i].value = min_int(sp[i].value + (1 * it->delta_time), sp[i].maxValue);
    }
}

void BcSystemsElementalsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystemsElementals);

    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);
    ECS_IMPORT(world, BcActors);
    ECS_IMPORT(world, BcElementals);

    // 2 different ego behaviors to alternate between moving and shifting plates; better way to do this?
    ECS_SYSTEM(world, EgoBehaviorTectonic, Planning,
        [in] Position,
        [inout] AngleRadians,
        [out] Destination,
        [none] (bc.actors.Ego, bc.actors.Ego.EgoTectonicSpirit)
    );

    ECS_SYSTEM(world, EgoBehaviorVolcano, Planning,
        [inout] SpiritPower,
        [none] (bc.actors.Ego, bc.actors.Ego.EgoVolcanoSpirit)
    );

    ECS_SYSTEM(world, ExecuteShiftPlates, Execution,
        [in] Position,
        [in] AngleRadians,
        [in] Plane(up(bc.planes.IsOn)),
        [in] PlateShiftStrength,
        [in] SpiritLifetime,
        [in] CreationTime,
        [in] Step($),
        [none] (bc.actors.Action, bc.actors.Action.ActionShiftPlates)
    );

    ECS_SYSTEM(world, ExecuteErupt, Execution,
        [in] Position,
        [in] Plane(up(bc.planes.IsOn)),
        [none] (bc.actors.Action, bc.actors.Action.ActionErupt)
    );

    ECS_SYSTEM(world, TickSpiritPower, AdvanceHour,
        [inout] SpiritPower,
        [none] bc.elementals.ElementalSpirit,
    );
}
