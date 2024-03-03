#include "systems_input.h"
#include "components/components_input.h"
#include <SDL2/SDL.h>

//static const char *AlphaNumNames[10] = {"Alpha0"};

// Default SDL key names with replacements to make the result safe to use as a Flecs entity name
const char *sanatizedGetKeyName(SDL_Keycode key) {
    // NOTE: might need changes for some keypad button names, already know that '<' and '>' break flecs entity name parsing
    // For now just skip everything except the keypad number keys
    // if (key > SDLK_KP_9 && key <= SDLK_KP_XOR) {
    //     ecs_warn("Unsupported keypad key: %s", SDL_GetKeyName(key));
    //     return NULL;
    // }
    // NOTE: might want changes to allow numbers to be bound in scripts, but can get around this by namespacing as Key.[0-9]
    // if (key >= SDLK_0 && key <= SDLK_9) {
    //     // return Alpha#
    //     return AlphaNumNames[key - SDLK_0];
    // }
    // Replaces the following SDL key names with CamelCased * from the SDL_SCANCODE_* enum identifier:
    // ''', '\', ',', '=', '`', '[', '-', '.', ']', ';', '/'
    switch(key) {
        case SDLK_QUOTE:
            return "Quote";
        case SDLK_BACKSLASH:
            return "Backslash";
        case SDLK_COMMA:
            return "Comma";
        case SDLK_EQUALS:
            return "Equals";
        case SDLK_BACKQUOTE:
            return "Grave";
        case SDLK_LEFTBRACKET:
            return "LeftBracket";
        case SDLK_MINUS:
            return "Minus";
        case SDLK_PERIOD:
            return "Period";
        case SDLK_RIGHTBRACKET:
            return "RightBracket";
        case SDLK_SEMICOLON:
            return "Semicolon";
        case SDLK_SLASH:
            return "Slash";
        case SDLK_KP_LESS:
            return "Keypad Less";
        case SDLK_KP_GREATER:
            return "Keypad Greater";
        case SDLK_UNKNOWN:
            ecs_warn("Unknown keycode: %i", key);
            return NULL;
        default:
            return SDL_GetKeyName(key);
    }
}

const char *getMouseButtonName(int button) {
    switch (button) {
        case 1:
            return "MouseLeft";
        case 2:
            return "MouseRight";
        case 3:
            return "MouseMiddle";
        case 4:
            return "Mouse4";
        case 5:
            return "Mouse5";
        default:
            return "Unknown";
    }
}

void createKeyEntitySDL(ecs_world_t *world, SDL_Scancode scancode) {
    KeyEntityMap *kem = ecs_singleton_get_mut(world, KeyEntityMap);
    // get key name from SDL, create entity, and add to map
    const char *keyname = sanatizedGetKeyName(SDL_SCANCODE_TO_KEYCODE(scancode));
    if (keyname == NULL) {
        // Don't create entities for nameless scancodes
        return;
    }
    ecs_entity_t keysParent = ecs_lookup_fullpath(world, "components.input.Key");
    ecs_entity_t keyEntity = ecs_new_from_path(world, keysParent, keyname);
    ecs_add(world, keyEntity, InputState);
    kem->entities[scancode] = keyEntity;
    ecs_singleton_modified(world, KeyEntityMap);
}

ecs_entity_t getKeyEntitySDL(ecs_world_t *world, SDL_KeyboardEvent key) {
    // check if key scancode is in map
    KeyEntityMap *kem = ecs_singleton_get_mut(world, KeyEntityMap);
    SDL_Scancode sc = key.keysym.scancode;
    ecs_entity_t keyEntity = kem->entities[sc];
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
    if (pressed) {
        ecs_set(world, buttonEntity, InputState, {INPUT_PRESSED | INPUT_HELD});
    } else {
        ecs_set(world, buttonEntity, InputState, {INPUT_RELEASED});
    }

    // search for matching binding, trigger system
    ecs_rule_t *r = ecs_singleton_get(world, BindingRules)->down;
    ecs_iter_t it = ecs_rule_iter(world, r);
    int32_t buttonVar = ecs_rule_find_var(r, "button");
    ecs_iter_set_var(&it, buttonVar, buttonEntity);
    while(ecs_rule_next(&it)) {
        ActionButton *actions = ecs_field(&it, ActionButton, 1);

        for (int i = 0; i < it.count; i++) {
            ActionButton action = actions[i];
            float dT = 0.016; // TODO: get this with the system query
            ecs_run(world, action.system, dT, NULL);
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

void AccumulateActionVectors(ecs_iter_t *it) {
    ActionVector *actions = ecs_field(it, ActionVector, 1);
    InputVector *inputs = ecs_field(it, InputVector, 2);
    ecs_entity_t button = ecs_pair_second(it->world, ecs_field_id(it, 2));
    const InputState *buttonState = ecs_get(it->world, button, InputState);

    if (buttonState != NULL) {
        if (*buttonState & INPUT_HELD) {
            for (int i = 0; i < it->count; i++) {
                ActionVector *action = &actions[i];
                InputVector input = inputs[i];
                action->vector.x += input.x;
                action->vector.y += input.y;
                action->vector.z += input.z;
            }
        }
    }
    // TODO: account for InputVector target being analog input
}

void RunActionVectors(ecs_iter_t *it) {
    ActionVector *actions = ecs_field(it, ActionVector, 1);

    for (int i = 0; i < it->count; i++) {
        ActionVector *action = &actions[i];
        if (fabsf(action->vector.x) > action->deadzone || fabsf(action->vector.y) > action->deadzone)
            ecs_run(it->world, action->system, it->delta_time, &action->vector);
        // TODO: do this at very start or end of frame
        action->vector = (InputVector){0};
    }
}

void SystemsInputImport(ecs_world_t *world) {
    ECS_MODULE(world, SystemsInput);

    ECS_IMPORT(world, ComponentsInput);

    // register backend-specific key to entity map and create singleton
    ecs_singleton_set(world, KeyEntityMap, {0}); // NB: must be 0-initialized

    // Create entities for every recognized input
    // Keyboard:
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        createKeyEntitySDL(world, i);
    }
    // Mouse: TODO
    // Touches: TODO
    // Controllers: TODO

    // rules used to find actions associated with an input
    ecs_rule_t *downRule = ecs_rule(world, {
        .expr = "ActionButton, (ButtonDown, $button)", // TODO: add terms to ignore disabled action groups
    });
    // TODO other rules
    ecs_rule_t *vectorRule = ecs_rule(world, {
        .expr = "ActionVector, (InputVector, $button)"
    });
    ecs_singleton_set(world, BindingRules, {downRule, 0, 0, vectorRule});

    ECS_SYSTEM(world, ProcessEvents, EcsPreFrame);
    // re-enable if you want the module to pump events for you
    ecs_enable(world, ProcessEvents, false);

    ECS_SYSTEM(world, AccumulateActionVectors, EcsPreFrame,
        [inout] ActionVector,
        [in] (InputVector, *)
    );
    ECS_SYSTEM(world, RunActionVectors, EcsPreUpdate,
        [in] ActionVector
    );
}

bool SystemsInput_ProcessEvent(ecs_world_t *world, const void *e) {
    return processEventSDL(world, e);
}
