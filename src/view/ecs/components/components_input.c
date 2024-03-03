#define COMPONENTS_INPUT_IMPL
#include "components_input.h"

void ComponentsInputImport(ecs_world_t *world) {
    ECS_MODULE(world, ComponentsInput);

    ECS_META_COMPONENT(world, KeyEntityMap);
    ECS_META_COMPONENT(world, BindingRules);

    ECS_TAG_DEFINE(world, ActionGroup);

    ECS_META_COMPONENT(world, InputState);
    ECS_META_COMPONENT(world, InputModifier);

    ECS_META_COMPONENT(world, ButtonDown);
    ECS_META_COMPONENT(world, ButtonHold);
    ECS_META_COMPONENT(world, ButtonUp);
    ECS_META_COMPONENT(world, ActionButton);
    ECS_META_COMPONENT(world, InputVector);
    ECS_META_COMPONENT(world, ActionVector);

    // Parents for button / other input entities to be created under later
    ecs_set_name(world, 0, "Key");
    ecs_set_name(world, 0, "Mouse");
    ecs_set_name(world, 0, "Touch");
    ecs_set_name(world, 0, "Controller1");
    ecs_set_name(world, 0, "Controller2");
    ecs_set_name(world, 0, "Controller3");
    ecs_set_name(world, 0, "Controller4");
    ecs_set_name(world, 0, "Joystick");
}
