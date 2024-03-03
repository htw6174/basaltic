#ifndef COMPONENTS_INPUT_H_INCLUDED
#define COMPONENTS_INPUT_H_INCLUDED

#include "flecs.h"

// Reflection system boilerplate to ensure meta symbols are only defined once
#undef ECS_META_IMPL
// Put before entity and primitive component declarations to avoid needing both extern declaration and definition
#undef META_DECL
#ifndef COMPONENTS_INPUT_IMPL
#define ECS_META_IMPL EXTERN
#define META_DECL extern
#else
#define META_DECL
#endif

// singleton component to store mapping of scancodes to entities
ECS_STRUCT(KeyEntityMap, {
    ecs_entity_t entities[512]; // size of SDL_NUM_SCANCODES, hopefully enough for other input backends also
});

// singleton component to store the rule used to find bindings by button
ECS_STRUCT(BindingRules, {
    ecs_rule_t *down;
    ecs_rule_t *hold;
    ecs_rule_t *up;
    ecs_rule_t *vector;
});

// Relationship used to enable and disable multiple bindings at once; when added to an entity with an InputBinding, if the target entity is disabled then the binding won't trigger
META_DECL ECS_TAG_DECLARE(ActionGroup);

ECS_BITMASK(InputState, {
    INPUT_DEFAULT =  0x00, // Treated the same as INPUT_PRESSED
    INPUT_PRESSED =  0x01,
    INPUT_RELEASED = 0x02,
    INPUT_HELD =     0x04
});

// Corresponds to SDL_Keymod values
ECS_BITMASK(InputModifier, {
    INPUT_KMOD_NONE = 0x0000,
    INPUT_KMOD_LSHIFT = 0x0001,
    INPUT_KMOD_RSHIFT = 0x0002,
    INPUT_KMOD_SHIFT = 0x0003,
    INPUT_KMOD_LCTRL = 0x0040,
    INPUT_KMOD_RCTRL = 0x0080,
    INPUT_KMOD_CTRL = 0x00c0,
    INPUT_KMOD_LALT = 0x0100,
    INPUT_KMOD_RALT = 0x0200,
    INPUT_KMOD_ALT = 0x0300,
    INPUT_KMOD_LGUI = 0x0400,
    INPUT_KMOD_RGUI = 0x0800,
    INPUT_KMOD_GUI = 0x0c00,
    INPUT_KMOD_NUM = 0x1000,
    INPUT_KMOD_CAPS = 0x2000,
    INPUT_KMOD_MODE = 0x4000,
    INPUT_KMOD_SCROLL = 0x8000,
    // Special value; when the EXCLUSIVE bit is set on a binding's kmod, it will only fire if exactly those modifier keys are held.
    // Otherwise, a binding will fire if _at least_ the set modifier keys are held
    // Note that this ignores the toggle keys NUM, CAPS, MODE, and SCROLL
    INPUT_KMOD_EXCLUSIVE = 0x10000
});

// ButtonDown/Hold/Up: relationships where the target is a key, mouse button, or controller button.
// Bind a button to an action by adding to an entity with ActionButton.
ECS_STRUCT(ButtonDown, {
    InputModifier kmod;
});

ECS_STRUCT(ButtonHold, {
    InputModifier kmod;
});

ECS_STRUCT(ButtonUp, {
    InputModifier kmod;
});

// Add ActionButton to an entity to represent an action users can make; add (Button*, [button]) relationships to bind inputs, and set .system to run a system when any binding is triggered.
ECS_STRUCT(ActionButton, {
    ecs_entity_t system;
});

// Relationship where the target is a key, button, or analog input.
// Bind these inputs to an action by adding to an entity with ActionVector.
// Set the direction and sensitivity of a bound input by setting one or more components, e.g. (Axis, Key.Right) {x: 1} maps the right arrow key to +x with sensitivity 1; (Axis, Key.Left) {x: -0.5} maps the left arrow key to -x with half the sensitivity.
// For analog inputs, axis mapping is done automatically but sensitivity and inversion can be overridden.
// e.g. (Axis, Mouse.Delta) {x: 1, y: 1} sets the default mouse binding; (Axis, Mouse.Delta) {x: 1.4, y: -1} increases horizontal sensitivity and inverts vertical sensitivity
ECS_STRUCT(InputVector, {
    float x;
    float y;
    float z;
});

// Add ActionVector to an entity for actions that rely on analog input; add (Axis, [button or input axis]) relationships to bind a button or analog input to one or more vector components.
ECS_STRUCT(ActionVector, {
    ecs_entity_t system;
    float deadzone;
    bool normalize;
    InputVector vector;
});

void ComponentsInputImport(ecs_world_t *world);

#endif // COMPONENTS_INPUT_H_INCLUDED
