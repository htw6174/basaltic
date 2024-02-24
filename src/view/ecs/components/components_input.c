#define COMPONENTS_INPUT_IMPL
#include "components_input.h"

void ComponentsInputImport(ecs_world_t *world) {
    ECS_MODULE(world, ComponentsInput);

    ECS_META_COMPONENT(world, KeyEntityMap);
    ECS_META_COMPONENT(world, BindingRule);

    ECS_TAG_DEFINE(world, InputBindGroup);

    ECS_META_COMPONENT(world, InputState);
    ECS_META_COMPONENT(world, InputModifier);

    ECS_META_COMPONENT(world, InputBinding);
    ECS_META_COMPONENT(world, Axis);
}
