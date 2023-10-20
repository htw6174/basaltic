#include "bc_elementals_systems.h"
#include "basaltic_phases.h"
#include "basaltic_components.h"

void EgoBehaviorTectonicMove(ecs_iter_t *it) {
    Position *pos = ecs_field(it, Position, 1);
    Destination *dest = ecs_field(it, Destination, 2);

    for (int i = 0; i < it->count; i++) {
        // TODO: move in similar direction
        dest[i] = htw_geo_addGridCoords(pos[i], htw_geo_hexGridDirections[HEX_DIRECTION_NORTH_EAST]);
        ecs_add_pair(it->world, it->entities[i], Action, ActionMove);
    }
}

void EgoBehaviorTectonicShift(ecs_iter_t *it) {

    for (int i = 0; i < it->count; i++) {
        ecs_add_pair(it->world, it->entities[i], Action, ActionShiftPlates);
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
    Plane *plane = ecs_field(it, Plane, 2);
    htw_ChunkMap *cm = plane->chunkMap;
    PlateShiftStrength *shiftStrength = ecs_field(it, PlateShiftStrength, 3);

    for (int i = 0; i < it->count; i++) {
        htw_geo_GridCoord posLeft = htw_geo_addGridCoords(positions[i], htw_geo_hexGridDirections[HEX_DIRECTION_NORTH_WEST]);
        CellData *left = htw_geo_getCell(cm, posLeft);
        left->height += shiftStrength[i].left;
        htw_geo_GridCoord posRight = htw_geo_addGridCoords(positions[i], htw_geo_hexGridDirections[HEX_DIRECTION_EAST]);
        CellData *right = htw_geo_getCell(cm, posRight);
        right->height += shiftStrength[i].right;
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
    ECS_SYSTEM(world, EgoBehaviorTectonicMove, Planning,
        [in] Position,
        [out] Destination,
        //[none] (bc.actors.Action, bc.actors.Action.ActionShiftPlates),
        [none] (bc.actors.Ego, bc.actors.Ego.EgoTectonicSpirit)
    );

    ECS_SYSTEM(world, EgoBehaviorTectonicShift, Planning,
        [in] Position,
        [none] (bc.actors.Action, bc.actors.Action.ActionMove),
        [none] (bc.actors.Ego, bc.actors.Ego.EgoTectonicSpirit)
    );

    ECS_SYSTEM(world, EgoBehaviorVolcano, Planning,
        [inout] SpiritPower,
        [none] (bc.actors.Ego, bc.actors.Ego.EgoVolcanoSpirit)
    );

    ECS_SYSTEM(world, ExecuteShiftPlates, Execution,
        [in] Position,
        [in] Plane(up(bc.planes.IsOn)),
        [in] PlateShiftStrength,
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
