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
    // constant source
    Step step = *ecs_field(it, Step, 3);

    static char *daySchedule[24] = {
        "ActionSleep", "ActionSleep", "ActionSleep", "ActionSleep", "ActionSleep", "ActionSleep",
        "ActionEatMeal", "ActionHunt", "ActionHunt", "ActionHunt", "ActionHunt", "ActionHunt",
        "ActionHunt", "ActionHunt", "ActionHunt", "ActionHunt", "ActionSocalize", "ActionEatMeal",
        "ActionSocalize", "ActionSocalize", "ActionSocalize", "ActionSocalize", "ActionSleep", "ActionSleep"
    };

    for (int i = 0; i < it->count; i++) {
        s32 hour = step % 24;
        ecs_entity_t action = ecs_lookup_child(it->world, Action, daySchedule[hour]);
        ecs_set_pair(it->world, it->entities[i], Action, action);
    }
}

void ExecuteHunt(ecs_iter_t *it) {

}

void ExecuteEatMeal(ecs_iter_t *it) {

}

void ExecuteSocalize(ecs_iter_t *it) {

}

void BcSystemsTribesImport(ecs_world_t *world) {
    ECS_MODULE(world, BcSystemsTribes);

    ECS_SYSTEM(world, EgoBehaviorVillager, Planning,
        [in] Condition,
        [inout] Stockpile(up(bc.tribes.MemberOf)),
        [in] Step($),
        [none] bc.actors.Ego.EgoVillager
    );

    ECS_SYSTEM(world, ExecuteHunt, Execution,
        [none] (bc.actors.Action, bc.actions.Action.ActionHunt)
    );

    ECS_SYSTEM(world, ExecuteEatMeal, Execution,
        [none] (bc.actors.Action, bc.actions.Action.ActionEatMeal)
    );

    ECS_SYSTEM(world, ExecuteSocalize, Execution,
        [none] (bc.actors.Action, bc.actions.Action.ActionSocalize)
    );
}
