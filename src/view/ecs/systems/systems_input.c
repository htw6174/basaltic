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
            return "Right";
        case 3:
            return "Middle";
        case 4:
            return "Button4";
        case 5:
            return "Button5";
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
    // ecs_entity_t keyEntity = ecs_entity(world, {
    //     .name = keyname,
    //     .add[0] = ecs_make_pair(EcsChildOf, Key)
    // });
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
    ecs_entity_t buttonEntity = ecs_entity(world, {
        .name = buttonName,
        .add[0] = ecs_make_pair(EcsChildOf, Mouse)
    });
    //ecs_new_from_path(world, Mouse, buttonName);
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
            ecs_set(world, MousePosition, InputVector, {e->motion.x, e->motion.y});
            buttonEntity = 0;
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

    for (int i = 0; i < it->count; i++) {
        deltas[i].previous = targetVec[i];
    }
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
    MousePosition = ecs_new_from_path(world, Mouse, "Position");
    MouseDelta = ecs_new_from_path(world, Mouse, "Delta");
    ecs_add(world, MousePosition, InputVector);
    ecs_add(world, MouseDelta, InputVector);
    ecs_add_pair(world, MouseDelta, ecs_id(DeltaVectorOf), MousePosition);
    // Touches: TODO
    // Controllers: TODO

    ecs_set_scope(world, oldScope);

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

    ECS_SYSTEM(world, CalcDeltas, EcsPreFrame,
        [out] InputVector,
        [in] (DeltaVectorOf, _)
        // TODO: after rule engine changes, can guarantee target has InputVector
    );

    ECS_SYSTEM(world, RunActionVectors, EcsPreUpdate,
        [in] ActionVector
    );

    ECS_SYSTEM(world, GetDeltaPrevious, EcsPostFrame,
        [out] (DeltaVectorOf, _)
    );
}

bool SystemsInput_ProcessEvent(ecs_world_t *world, const void *e) {
    return processEventSDL(world, e);
}
