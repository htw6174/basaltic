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
ECS_STRUCT(BindingRule, {
    ecs_rule_t *rule;
});

// Relationship used to enable and disable multiple bindings at once; when added to an entity with an InputBinding, if the target entity is disabled then the binding won't trigger
META_DECL ECS_TAG_DECLARE(InputBindGroup);

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
    INPUT_KMOD_SCROLL = 0x8000

});

ECS_STRUCT(InputBinding, {
    ecs_entity_t system; // system to run when binding is triggered
    InputState trigger; // state the key must be in for the binding to trigger; can OR together multiple states. Default: Pressed
    InputModifier modifiers; // modifier combo required for binding to trigger; can OR together multiple states. Default: None
});

// Axis direction to pass along from a button press. Useful when making systems that can be triggered by both buttons and joysticks
ECS_STRUCT(Axis, {
    float x;
    float y;
});

// Data passed as system context when a binding is triggered
// If binding is an analog input, both current axies and delta will be set
// If binding is a button, axis x&y will be set to the binding's x&y
typedef struct InputContext {
    float axis_x;
    float axis_y;
    float delta_x;
    float delta_y;
} InputContext;

void ComponentsInputImport(ecs_world_t *world);

#endif // COMPONENTS_INPUT_H_INCLUDED
