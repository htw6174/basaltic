#include "bc_systems_tribes.h"
#include "basaltic_phases.h"
#include "basaltic_components.h"

void StockpileTickDay(ecs_iter_t *it) {
    Stockpile *stockpiles = ecs_field(it, Stockpile, 1);

    for (int i = 0; i < it->count; i++) {
        // TODO: spoil food over time
    }
}

void EgoBehaviorVillager(ecs_iter_t *it) {
    Condition *conditions = ecs_field(it, Condition, 1);
    Stockpile *stockpiles = ecs_field(it, Stockpile, 2);

    for (int i = 0; i < it->count; i++) {
        if (conditions[i].stamina < conditions[i].maxStamina / 2) {
            // Eat when at half stamina

        } else {
            // Do work
        }
    }
}

void BcSystemsTribesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystemsTribes);

    ECS_SYSTEM(world, EgoBehaviorVillager, Planning,
        [in] Condition,
        [inout] Stockpile(up(bc.tribes.MemberOf)),
        [none] bc.actors.Ego.EgoVillager
    );
}
