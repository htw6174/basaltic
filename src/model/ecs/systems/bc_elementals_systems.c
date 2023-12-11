#include "bc_elementals_systems.h"
#include "basaltic_phases.h"
#include "basaltic_components.h"
#include <math.h>

/// 0 rad == HEX_DIR_NORTH_EAST, continues clockwise
HexDirection radToHexDir(AngleRadians angle);

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

void shiftTerrainInLine(htw_ChunkMap *cm, htw_geo_GridCoord start, htw_geo_GridCoord towards, s32 baseline, s32 strength, float falloff) {
    falloff = fmaxf(falloff, 1.0);
    s32 strengthPool = (strength * strength) / falloff;

    const int RANGE_MAX = 100; // ensure crazy strength values don't cause issues
    int i = 0;
    while(strengthPool > 0 && i < RANGE_MAX) {
        htw_geo_GridCoord lineCoord = hexOnLine(start, towards, i);
        CellData *cell = htw_geo_getCell(cm, lineCoord);
        // amount of strength that can be used is limited by how far the next cell is above the faultline's current position
        s32 heightDifference = cell->height - baseline;
        // first step always = strength
        s32 usableStrength = roundf((float)strengthPool * (falloff / strength));
        float heightStrengthRatio = (float)heightDifference / usableStrength;
        const float heightFalloffFactor = 4.0;
        float limiter = fmaxf(0, heightStrengthRatio * heightFalloffFactor);
        // limiter = 0 -> limitFactor = 1
        // limiter = 4 -> limitFactor = 0.2
        // limiter = MAX -> limitFactor => 0
        float limitFactor = 1.0 / (limiter + 1.0);
        // Always round away from 0 so that the strengthPool always decreases
        s32 expendedStrength = strength > 0 ? ceil(usableStrength * limitFactor) : floor(usableStrength * limitFactor);
        s32 unclampedHeight = expendedStrength + cell->height;
        cell->height = CLAMP(unclampedHeight, INT8_MIN, INT8_MAX);
        // only subtract as much strength as was used
        strengthPool -= abs(expendedStrength);
        i++;
    }
}

void landslideParticle(htw_ChunkMap *cm, htw_geo_GridCoord start) {
    htw_geo_GridCoord pos = start;

    // Get all cells around current position
    // If one of them is at least 5 down from current, move there and repeat
    // Otherwise, increase height here by 1 and end
    //const int MAX_SLOPE = 3;
    int maxSlope = htw_randIntRange(2, 6);
    bool sliding = true;
    while(sliding) {
        CellData *cell = htw_geo_getCell(cm, pos);
        int greatestDrop = maxSlope;
        htw_geo_GridCoord candidate = pos;
        for (int i = 0; i < HEX_DIRECTION_COUNT; i++) {
            htw_geo_GridCoord sample = POSITION_IN_DIRECTION(pos, i);
            CellData *sampleCell = htw_geo_getCell(cm, sample);
            int drop = cell->height - sampleCell->height;
            if (drop > greatestDrop) {
                candidate = sample;
                greatestDrop = drop;
            }
            // Wipe out vegetation and add some nutrient (groundwater?)
            sampleCell->understory = 0;
            sampleCell->canopy = 0;
            s64 groundwater = sampleCell->groundwater;
            groundwater += 150;
            sampleCell->groundwater = MIN(groundwater, INT16_MAX);
        }
        sliding = greatestDrop > maxSlope;
        pos = candidate;
    }

    CellData *cell = htw_geo_getCell(cm, pos);
    cell->height += 1;

    //htw_geo_GridCoord forward = POSITION_IN_DIRECTION(pos, radToHexDir(angle));
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
            if (htw_coinFlip()) {
                angle[i] += PI/180.0;
            }
            dest[i] = POSITION_IN_DIRECTION(pos[i], radToHexDir(angle[i] + htw_coinFlip()));
            ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
        } else {
            ecs_add_pair(it->world, it->entities[i], Action, ActionShiftPlates);
        }
    }
}

void EgoBehaviorVolcano(ecs_iter_t *it) {
    SpiritPower *sp = ecs_field(it, SpiritPower, 1);

    for (int i = 0; i < it->count; i++) {
        if (sp[i].value >= 1) {
            sp[i].value -= 1;
            ecs_add_pair(it->world, it->entities[i], Action, ActionErupt);
        } else {
            ecs_add_pair(it->world, it->entities[i], Action, ActionIdle);
        }
    }
}

void EgoBehaviorStorm(ecs_iter_t *it) {
    Position *pos = ecs_field(it, Position, 1);
    Destination *dest = ecs_field(it, Destination, 2);

    for (int i = 0; i < it->count; i++) {
        u32 hash = xxh_hash2d(it->entities[i], pos[i].x, pos[i].y);
        s32 wind = (hash % 3) - 1; // TODO weight towards 0
        HexDirection dir = MOD(HEX_DIRECTION_EAST + wind, HEX_DIRECTION_COUNT);
        dest[i] = POSITION_IN_DIRECTION(pos[i], dir);
        ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
    }
}

void PrepStorm(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Destination *destinations = ecs_field(it, Destination, 2);
    SpiritPower *spiritPowers = ecs_field(it, SpiritPower, 3);
    StormCloud *stormClouds = ecs_field(it, StormCloud, 4);
    // constant sources
    Plane *plane = ecs_field(it, Plane, 5);
    htw_ChunkMap *cm = plane->chunkMap;
    Climate *climate = ecs_field(it, Climate, 6);

    for (int i = 0; i < it->count; i++) {
        Position pos = positions[i];
        SpiritPower *sp = &spiritPowers[i];
        StormCloud *storm = &stormClouds[i];
        CellData *cell = htw_geo_getCell(cm, pos);
        CellData *destCell = htw_geo_getCell(cm, destinations[i]);
        // get temperature in centicelsius TODO: factor in
        s32 temp = plane_GetCellTemperature(plane, climate, pos);
        // Get slope between current and destination cell, only matters if destination isn't over the ocean
        s32 slope = destCell->height < 0 ? 0 : destCell->height - cell->height;
        if (cell->height < 0) {
            // over ocean
            sp->value = MIN(sp->value + 1000, sp->maxValue);
        } else {
            // over land
        }

        if (storm->isStorming) {
            // apply rain
            // less accumulated energy = smaller storm area
            u32 radius = ((float)(storm->radius * sp->value) / sp->maxValue) + 1;
            u32 area = htw_geo_getHexArea(radius);
            htw_geo_CubeCoord cubeCoord = {0, 0, 0};
            for (int i = 0; i < area; i++) {
                htw_geo_GridCoord gridCoord = htw_geo_cubeToGridCoord(cubeCoord);
                htw_geo_GridCoord worldCoord = htw_geo_addGridCoords(pos, gridCoord);
                CellData *c = htw_geo_getCell(cm, worldCoord);
                // give 1% spirit power to cell as surfacewater
                s64 surfacewater = c->surfacewater;
                s32 rainfall = MAX(1, sp->value * 0.01);
                surfacewater += rainfall * 5;
                sp->value -= rainfall;
                c->surfacewater = MIN(surfacewater, UINT16_MAX);

                htw_geo_getNextHexSpiralCoord(&cubeCoord);
            }

            // determine if storm should end
            s32 dc = storm->maxDuration - storm->currentDuration;
            // 5% increased difficulty per unit of slope
            dc += slope * storm->maxDuration * 0.05;
            s32 roll = htw_rtd(1, storm->maxDuration, NULL);
            if (roll >= dc || sp->value <= 0) {
                storm->isStorming = false;
                storm->currentDuration = 0;
            } else {
                storm->currentDuration += 1;
            }
        } else {
            // determine if storm should start
            s32 dc = storm->maxDuration - storm->currentDuration;
            // 5% decreased difficulty per unit of slope
            dc -= slope * storm->maxDuration * 0.05;
            // 50% increased difficulty if below max power
            // up to 100% increased difficulty depending on available power
            dc += ((float)(storm->maxDuration * sp->value) / sp->maxValue);
            //dc += sp->value < sp->maxValue ? storm->maxDuration * 0.5 : 0;
            s32 roll = htw_rtd(1, storm->maxDuration, NULL);
            if (roll >= dc) {
                storm->isStorming = true;
                storm->currentDuration = 0;
            } else {
                storm->currentDuration += 1;
            }
        }
    }
}

void ExecuteShiftPlates(ecs_iter_t *it) {
    int f = 0;
    Position *positions = ecs_field(it, Position, ++f);
    Elevation *elevation = ecs_field(it, Elevation, ++f);
    AngleRadians *angle = ecs_field(it, AngleRadians, ++f);
    Plane *plane = ecs_field(it, Plane, ++f);
    htw_ChunkMap *cm = plane->chunkMap;
    PlateShiftStrength *shiftStrength = ecs_field(it, PlateShiftStrength, ++f);
    SpiritLifetime *lifetime = ecs_field(it, SpiritLifetime, ++f);
    CreationTime *createTime = ecs_field(it, CreationTime, ++f);
    Step step = *ecs_field(it, Step, ++f);

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

        // Move elevation towards current cell height
        CellData *c = htw_geo_getCell(cm, positions[i]);
        s32 cellHeight = c->height;
        s32 diff = cellHeight - elevation[i];
        diff = CLAMP(diff, -1, 1);
        elevation[i] += diff;

        // Left side
        htw_geo_GridCoord startLeft = positions[i];
        htw_geo_GridCoord endLeft = hexLineEndPoint(startLeft, 20.0, angle[i] + (PI/2.0));
        shiftTerrainInLine(cm, startLeft, endLeft, elevation[i], shiftStrength[i].left * strengthFactor, shiftStrength[i].falloff);

        // Right side (forward turned 60 deg CW)
        htw_geo_GridCoord startRight = POSITION_IN_DIRECTION(positions[i], radToHexDir(angle[i] - (PI/3.0)));
        htw_geo_GridCoord endRight = hexLineEndPoint(startRight, 20.0, angle[i] - (PI/2.0));
        shiftTerrainInLine(cm, startRight, endRight, elevation[i], shiftStrength[i].right * strengthFactor, shiftStrength[i].falloff);
    }
}

void ExecuteErupt(ecs_iter_t *it) {
    Position *positions = ecs_field(it, Position, 1);
    Elevation *elevation = ecs_field(it, Elevation, 2);
    Plane *plane = ecs_field(it, Plane, 3);
    htw_ChunkMap *cm = plane->chunkMap;
    SpiritLifetime *lifetime = ecs_field(it, SpiritLifetime, 4);
    CreationTime *createTime = ecs_field(it, CreationTime, 5);
    Step step = *ecs_field(it, Step, 6);

    for (int i = 0; i < it->count; i++) {
        u64 age = step - createTime[i];
        SpiritLifetime lt = lifetime[i];
        s32 maturityEnd = lt.youngSteps + lt.matureSteps;
        s32 lifeEnd = maturityEnd + lt.oldSteps;
        if (age > lifeEnd) {
            // End of life, destroy
            ecs_delete(it->world, it->entities[i]);
        }

        CellData *cell = htw_geo_getCell(cm, positions[i]);

        htw_geo_GridCoord start = positions[i];
        //htw_geo_GridCoord end = hexLineEndPoint(start, 20.0, angle);
        //shiftTerrainInLine(cm, start, end, elevation[i], 4, 1.0);
        landslideParticle(cm, start);
    }
}

void TickSpiritPower(ecs_iter_t *it) {
    SpiritPower *sp = ecs_field(it, SpiritPower, 1);

    for (int i = 0; i < it->count; i++) {
        int gain = sp[i].value + it->delta_time;
        sp[i].value = MIN(gain, sp[i].maxValue);
    }
}

void BcSystemsElementalsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystemsElementals);

    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);
    ECS_IMPORT(world, BcActors);
    ECS_IMPORT(world, BcElementals);

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

    ECS_SYSTEM(world, EgoBehaviorStorm, Planning,
        [in] Position,
        [out] Destination,
        [none] (bc.actors.Ego, bc.actors.Ego.EgoStormSpirit)
    );

    ECS_SYSTEM(world, PrepStorm, Prep,
        [in] Position,
        [in] Destination,
        [inout] SpiritPower,
        [inout] StormCloud,
        [in] Plane(up(bc.planes.IsIn)),
        [in] Climate(up(bc.planes.IsIn))
    );

    ECS_SYSTEM(world, ExecuteShiftPlates, Execution,
        [in] Position,
        [inout] Elevation,
        [in] AngleRadians,
        [in] Plane(up(bc.planes.IsIn)),
        [in] PlateShiftStrength,
        [in] SpiritLifetime,
        [in] CreationTime,
        [in] Step($),
        [none] (bc.actors.Action, bc.actors.Action.ActionShiftPlates)
    );

    ECS_SYSTEM(world, ExecuteErupt, Execution,
        [in] Position,
        [in] Elevation,
        [in] Plane(up(bc.planes.IsIn)),
        [in] SpiritLifetime,
        [in] CreationTime,
        [in] Step($),
        [none] (bc.actors.Action, bc.actors.Action.ActionErupt)
    );

    ECS_SYSTEM(world, TickSpiritPower, AdvanceStep,
        [inout] SpiritPower,
        [none] bc.elementals.ElementalSpirit,
    );
}
