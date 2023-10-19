#include "bc_elementals_systems.h"
#include "basaltic_phases.h"
#include "basaltic_components.h"

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
        sp[i].value = min_int(sp[i].value + 1, sp[i].maxValue);
    }
}

void BcSystemsElementalsImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystemsElementals);

    ECS_IMPORT(world, BcPhases);
    ECS_IMPORT(world, BcPlanes);
    ECS_IMPORT(world, BcActors);
    ECS_IMPORT(world, BcElementals);

    ECS_SYSTEM(world, EgoBehaviorVolcano, Planning,
        [inout] SpiritPower,
        [none] (bc.actors.Ego, bc.actors.Ego.EgoVolcanoSpirit)
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
