#include "systems_input.h"
#include "components/components_input.h"
#include <SDL2/SDL.h>

// singleton component to store mapping of scancodes to entities
typedef struct  {
    ecs_entity_t entities[SDL_NUM_SCANCODES];
} KeyEntityMap;
ECS_COMPONENT_DECLARE(KeyEntityMap);

typedef struct {
    ecs_entity_t entities[5]; // left, middle, right, and 2 side buttons
} MouseEntityMap;
ECS_COMPONENT_DECLARE(MouseEntityMap);

// Input sources; short names are useful for plecs scripts, but too general to put in the components header
ECS_ENTITY_DECLARE(Key);
ECS_ENTITY_DECLARE(Mouse);
ECS_ENTITY_DECLARE(Touch);
ECS_ENTITY_DECLARE(Controller);
ECS_ENTITY_DECLARE(Joystick);

// Convient access to axies
ECS_ENTITY_DECLARE(MousePosition);
ECS_ENTITY_DECLARE(MouseDelta);
ECS_ENTITY_DECLARE(MouseWheel);

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
            return "Left";
        case 2:
            return "Middle";
        case 3:
            return "Right";
        case 4:
            return "4";
        case 5:
            return "5";
        default:
            ecs_warn("Unknown mouse button index: %i", button);
            return NULL;
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
    ecs_entity_t keyEntity = ecs_new_from_path(world, Key, keyname);
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

void createMouseEntitySDL(ecs_world_t *world, int button) {
    MouseEntityMap *mem = ecs_singleton_get_mut(world, MouseEntityMap);
    const char *buttonName = getMouseButtonName(button);
    ecs_entity_t buttonEntity = ecs_new_from_path(world, Mouse, buttonName);
    ecs_add(world, buttonEntity, InputState);
    mem->entities[button] = buttonEntity;
    ecs_singleton_modified(world, MouseEntityMap);
}

ecs_entity_t getMouseEntitySDL(ecs_world_t *world, SDL_MouseButtonEvent button) {
    MouseEntityMap *mem = ecs_singleton_get_mut(world, MouseEntityMap);
    ecs_entity_t buttonEntity = mem->entities[button.button];
    return buttonEntity;
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
        case SDL_CONTROLLERBUTTONDOWN:
            pressed = true;
        case SDL_CONTROLLERBUTTONUP:
            // TODO
            buttonEntity = 0;
            ecs_warn("Controllers not yet supported!");
        case SDL_JOYBUTTONDOWN:
            pressed = true;
        case SDL_JOYBUTTONUP:
            // TODO
            buttonEntity = 0;
            ecs_warn("Joystick not yet supported!");
            break;
        case SDL_MOUSEMOTION:
            // No button to process, update mouse vectors
            ecs_set(world, MouseDelta, InputVector, {e->motion.xrel, e->motion.yrel});
            buttonEntity = 0;
            break;
        case SDL_MOUSEWHEEL:
            ecs_set(world, MouseWheel, InputVector, {e->wheel.x, e->wheel.y});
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
    ecs_rule_t *r;
    if (pressed) {
        r = ecs_singleton_get(world, BindingRules)->down;
    } else {
        r = ecs_singleton_get(world, BindingRules)->up;
    }
    ecs_iter_t it = ecs_rule_iter(world, r);
    int32_t buttonVar = ecs_rule_find_var(r, "button");
    ecs_iter_set_var(&it, buttonVar, buttonEntity);
    while(ecs_rule_next(&it)) {
        ActionButton *actions = ecs_field(&it, ActionButton, 1);

        for (int i = 0; i < it.count; i++) {
            actions[i].activated = true;
            if (ecs_field_is_set(&it, 3)) {
                // TODO: pass action context to immediate run systems
                ecs_run(world, actions[i].system, 0.0, NULL);
            }
        }
    }
    return true;
}

void ProcessEvents(ecs_iter_t *it) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        processEventSDL(it->world, &e);
        // TODO: check for callback components added to ProcessEvents for unhandled events and pass-along
    }
}

void CalcAxisDeltas(ecs_iter_t *it) {
    InputVector *vectors = ecs_field(it, InputVector, 1);

    for (int i = 0; i < it->count; i++) {
        // TODO
    }
}

void AccumulateActionVectors(ecs_iter_t *it) {
    ActionVector *actions = ecs_field(it, ActionVector, 1);
    InputVector *inputs = ecs_field(it, InputVector, 2);
    ecs_entity_t button = ecs_pair_second(it->world, ecs_field_id(it, 2));
    const InputState *buttonState = ecs_get(it->world, button, InputState);
    const InputVector *vectorState = ecs_get(it->world, button, InputVector);

    for (int i = 0; i < it->count; i++) {
        ActionVector *action = &actions[i];
        InputVector input = inputs[i];
        if (buttonState != NULL) {
            // target is button input
            if (*buttonState & INPUT_HELD) {
                action->vector.x += input.x;
                action->vector.y += input.y;
                action->vector.z += input.z;
            }
        } else if (vectorState != NULL) {
            // target is analog input
            action->vector.x += input.x * vectorState[i].x;
            action->vector.y += input.y * vectorState[i].y;
            action->vector.z += input.z * vectorState[i].z;
        }
    }
}

void RunTriggeredActions(ecs_iter_t *it) {
    ActionButton *actions = ecs_field(it, ActionButton, 1);

    if (ecs_field_is_set(it, 2)) {
        ecs_entity_t contextType = ecs_pair_second(it->world, ecs_field_id(it, 2));

        for (int i = 0; i < it->count; i++) {
            ActionButton action = actions[i];
            if (action.activated) {
                // Get action context to pass to system
                // Because system query doesn't know what ActionContext target is, have to get directly from current entity
                void *context = ecs_get_mut_id(it->world, it->entities[i], ecs_make_pair(ActionParams, contextType));
                ecs_run(it->world, action.system, it->delta_time, context);
            }
        }
    } else {
        for (int i = 0; i < it->count; i++) {
            ActionButton action = actions[i];
            if (action.activated) {
                ecs_run(it->world, action.system, it->delta_time, NULL);
            }
        }
    }
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

void CalcDeltas(ecs_iter_t *it) {
    InputVector *vectors = ecs_field(it, InputVector, 1);
    DeltaVectorOf *deltas = ecs_field(it, DeltaVectorOf, 2);
    ecs_entity_t target = ecs_pair_second(it->world, ecs_field_id(it, 2));
    const InputVector *targetVec = ecs_get(it->world, target, InputVector);

    for (int i = 0; i < it->count; i++) {
        InputVector current = targetVec[i];
        InputVector previous = deltas[i].previous;
        vectors[i] = (InputVector){
            current.x - previous.x,
            current.y - previous.y,
            current.z - previous.z,
        };
    }
}

void GetDeltaPrevious(ecs_iter_t *it) {
    DeltaVectorOf *deltas = ecs_field(it, DeltaVectorOf, 1);
    ecs_entity_t target = ecs_pair_second(it->world, ecs_field_id(it, 1));
    const InputVector *targetVec = ecs_get(it->world, target, InputVector);

    if (targetVec != NULL) {
        for (int i = 0; i < it->count; i++) {
            deltas[i].previous = targetVec[i];
        }
    }
}

void ResetInputButtons(ecs_iter_t *it) {
    InputState *states = ecs_field(it, InputState, 1);

    for (int i = 0; i < it->count; i++) {
        states[i] &= INPUT_HELD;
    }
}

void ResetInputVectors(ecs_iter_t *it) {
    InputVector *vectors = ecs_field(it, InputVector, 1);

    for (int i = 0; i < it->count; i++) {
        vectors[i] = (InputVector){0};
    }
}

void ResetActionButtons(ecs_iter_t *it) {
    ActionButton *actions = ecs_field(it, ActionButton, 1);

    for (int i = 0; i < it->count; i++) {
        actions[i].activated = false;
    }
}

void ResetActionVectors(ecs_iter_t *it) {
    ActionVector *actions = ecs_field(it, ActionVector, 1);

    for (int i = 0; i < it->count; i++) {
        actions[i].vector = (InputVector){0};
    }
}

void EcsEnable(ecs_iter_t *it) {
    ecs_entity_t target = *(ecs_entity_t*)it->param;
    ecs_enable(it->world, target, true);
}

void EcsDisable(ecs_iter_t *it) {
    ecs_entity_t target = *(ecs_entity_t*)it->param;
    ecs_enable(it->world, target, false);
}

void SystemsInputImport(ecs_world_t *world) {
    ECS_MODULE(world, SystemsInput);

    ECS_IMPORT(world, ComponentsInput);

    // register backend-specific button to entity map and create singleton
    ECS_COMPONENT_DEFINE(world, KeyEntityMap);
    ECS_COMPONENT_DEFINE(world, MouseEntityMap);
    ecs_singleton_set(world, KeyEntityMap, {0}); // NB: must be 0-initialized
    ecs_singleton_set(world, MouseEntityMap, {0}); // NB: must be 0-initialized

    // Use the components module scope to create button and axis entities
    ecs_entity_t componentsModule = ecs_lookup_fullpath(world, "components.input");
    ecs_entity_t oldScope = ecs_set_scope(world, componentsModule);

    // Entities to hold different input categories
    ECS_ENTITY_DEFINE(world, Key);
    ECS_ENTITY_DEFINE(world, Mouse);
    ECS_ENTITY_DEFINE(world, Touch);
    ECS_ENTITY_DEFINE(world, Controller);
    ECS_ENTITY_DEFINE(world, Joystick);

    // Create entities for every recognized input
    // Keyboard:
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        createKeyEntitySDL(world, i);
    }
    // Mouse:
    for (int i = 1; i < 6; i++) {
        createMouseEntitySDL(world, i);
    }
    // Pixel coordinates directly from SDL; origin at top-left
    MousePosition = ecs_new_from_path(world, Mouse, "Position");
    MouseDelta = ecs_new_from_path(world, Mouse, "Delta");
    MouseWheel = ecs_new_from_path(world, Mouse, "Wheel");
    ecs_add(world, MousePosition, InputVector);
    ecs_add(world, MouseDelta, InputVector);
    ecs_add_pair(world, MouseDelta, ecs_id(DeltaVectorOf), MousePosition);
    ecs_add(world, MouseWheel, InputVector);
    // Touches: TODO
    // Controllers: TODO

    ecs_set_scope(world, oldScope);

    // rules used to find actions associated with an input
    ecs_rule_t *downRule = ecs_rule(world, {
        .expr = "ActionButton, (ButtonDown, $button), ?components.input.ImmediateAction", // TODO: add terms to ignore disabled action groups
    });
    ecs_rule_t *upRule = ecs_rule(world, {
        .expr = "ActionButton, (ButtonUp, $button), ?components.input.ImmediateAction", // TODO: add terms to ignore disabled action groups
    });
    // TODO other rules
    ecs_rule_t *vectorRule = ecs_rule(world, {
        .expr = "ActionVector, (InputVector, $button), ?components.input.ImmediateAction"
    });
    ecs_singleton_set(world, BindingRules, {downRule, 0, upRule, vectorRule});

    ECS_SYSTEM(world, ProcessEvents, EcsOnLoad);
    // re-enable if you want the module to pump events for you
    ecs_enable(world, ProcessEvents, false);

    ECS_SYSTEM(world, AccumulateActionVectors, EcsPostLoad,
        [inout] ActionVector,
        [in] (InputVector, *)
    );

    ECS_SYSTEM(world, CalcDeltas, EcsPostLoad,
        [out] InputVector,
        [in] (DeltaVectorOf, _)
        // TODO: after rule engine changes, can use var to guarantee target has InputVector. Can't use relationship traversal becuase also need value of DeltaVectorOf component (or can I query for both?)
    );

    ECS_SYSTEM(world, RunTriggeredActions, EcsPostLoad,
        [in] ActionButton,
        [in] ?(components.input.ActionParams, _),
        [none] !components.input.ImmediateAction,
        [none] !Disabled(up(components.input.ActionGroup))
    );

    ECS_SYSTEM(world, RunActionVectors, EcsPostLoad,
        [in] ActionVector,
        [none] !components.input.ImmediateAction,
        [none] !Disabled(up(components.input.ActionGroup))
    );

    ECS_SYSTEM(world, GetDeltaPrevious, EcsPostFrame,
        [out] (DeltaVectorOf, _)
    );

    ECS_SYSTEM(world, ResetInputButtons, EcsPostFrame,
        [inout] InputState
    );

    ECS_SYSTEM(world, ResetInputVectors, EcsPostFrame,
        [out] InputVector
    );

    ECS_SYSTEM(world, ResetActionButtons, EcsPostFrame,
        [out] ActionButton
    );

    ECS_SYSTEM(world, ResetActionVectors, EcsPostFrame,
        [out] ActionVector
    );

    // builtin Action Systems for common ECS actions
    ECS_SYSTEM(world, EcsEnable, 0);
    ECS_SYSTEM(world, EcsDisable, 0);
}

bool SystemsInput_ProcessEvent(ecs_world_t *world, const void *e) {
    return processEventSDL(world, e);
}
