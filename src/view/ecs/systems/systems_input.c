#include "systems_input.h"
#include "components/components_input.h"
#include <SDL2/SDL.h>

// Default SDL key names with a few replacements to make the result safe to use as a Flecs entity name
const char *sanatizedGetKeyName(SDL_Keycode key) {
    // TODO
    // No names which are only numbers, they get interpreted as entity IDs
    // No "special characters" (!, %, *, etc.)
    // No whitespace
    return SDL_GetKeyName(key);
}

const char *getMouseButtonName(int button) {
    switch (button) {
        case 1:
            return "mouse_left";
        case 2:
            return "mouse_right";
        case 3:
            return "mouse_middle";
        case 4:
            return "mouse_4";
        case 5:
            return "mouse_5";
        default:
            return "";
    }
}

ecs_entity_t getKeyEntitySDL(ecs_world_t *world, SDL_KeyboardEvent key) {
    // check if key scancode is in map
    KeyEntityMap *kem = ecs_singleton_get_mut(world, KeyEntityMap);
    SDL_Scancode sc = key.keysym.scancode;
    ecs_entity_t keyEntity = kem->entities[sc];
    if (keyEntity == 0) {
        // get key name from SDL, create entity, and add to map
        const char *keyname = sanatizedGetKeyName(key.keysym.sym);
        ecs_entity_t module = ecs_lookup_fullpath(world, "components.input");
        keyEntity = ecs_new_from_path(world, module, keyname);
        ecs_add(world, keyEntity, InputState);
        kem->entities[sc] = keyEntity;
        ecs_singleton_modified(world, KeyEntityMap);
    }
    return keyEntity;
}

ecs_entity_t getMouseEntitySDL(ecs_world_t *world, SDL_MouseButtonEvent button) {
    // TODO
    return 0;
}

bool processEventSDL(ecs_world_t *world, const SDL_Event *e) {
    bool pressed = false;
    ecs_entity_t buttonEntity = 0;
    switch (e->type) {
        case SDL_KEYDOWN:
            pressed = true;
        case SDL_KEYUP:
            buttonEntity = getKeyEntitySDL(world, e->key);
            break;
        case SDL_MOUSEBUTTONDOWN:
            pressed = true;
        case SDL_MOUSEBUTTONUP:
            buttonEntity = getMouseEntitySDL(world, e->button);
            break;
        case SDL_JOYBUTTONDOWN:
            pressed = true;
        case SDL_JOYBUTTONUP:
            buttonEntity = 0;
            ecs_warn("Controller not yet supported!");
            break;
        case SDL_MOUSEMOTION:
            // TODO
            break;
        default:
            return false;
    }

    if (buttonEntity == 0) {
        return false;
    }

    // update button with event info
    ecs_set(world, buttonEntity, InputState, {e->key.state});

    // search for matching binding, trigger system
    ecs_rule_t *r = ecs_singleton_get(world, BindingRule)->rule;
    ecs_iter_t it = ecs_rule_iter(world, r);
    int32_t buttonVar = ecs_rule_find_var(r, "button");
    ecs_iter_set_var(&it, buttonVar, buttonEntity);
    while(ecs_rule_next(&it)) {
        InputBinding *binds = ecs_field(&it, InputBinding, 1);
        Axis *axies = ecs_field(&it, Axis, 2);

        for (int i = 0; i < it.count; i++) {
            InputBinding bind = binds[i];
            Axis axis = {0, 0};
            if (axies != NULL) {
                axis = axies[i];
            }
            InputContext ic = {
                .axis_x = axis.x,
                .axis_y = axis.y,
                .delta_x = 0,
                .delta_y = 0
            };
            // PROBLEM: if this isn't run in the main pipeline, there isn't a 0-assumptions way to get delta time
            float dT = 0.016;
            ecs_run(world, bind.system, dT, &ic);
        }
    }
    return 0;
}

void ProcessEvents(ecs_iter_t *it) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        processEventSDL(it->world, &e);
        // TODO: check for callback components added to ProcessEvents for unhandled events and pass-along
    }
}

void SystemsInputImport(ecs_world_t *world) {
    ECS_MODULE(world, SystemsInput);

    ECS_IMPORT(world, ComponentsInput);

    // register backend-specific key to entity map and create singleton
    ecs_singleton_set(world, KeyEntityMap, {0}); // NB: must be 0-initialized
    // rule used to find bindings on a specific button
    ecs_rule_t *r = ecs_rule(world, {
        .expr = "InputBinding, ?Axis, (ChildOf, $button)", //, (BindGroup, $group), !Disabled($group)" // TODO: add bind group terms
    });
    ecs_singleton_set(world, BindingRule, {r});

    ECS_SYSTEM(world, ProcessEvents, EcsPreUpdate);
    // re-enable if you want the module to pump events for you
    ecs_enable(world, ProcessEvents, false);
}

bool SystemsInput_ProcessEvent(ecs_world_t *world, const void *e) {
    return processEventSDL(world, e);
}
