#define COMPONENTS_INPUT_IMPL
#include "components_input.h"

void ComponentsInputImport(ecs_world_t *world) {
    ECS_MODULE(world, ComponentsInput);

    ECS_META_COMPONENT(world, BindingRules);

    ECS_TAG_DEFINE(world, ActionGroup);
    ECS_TAG_DEFINE(world, ImmediateAction);

    ECS_META_COMPONENT(world, InputState);
    ECS_META_COMPONENT(world, InputModifier);

    ECS_META_COMPONENT(world, ButtonDown);
    ECS_META_COMPONENT(world, ButtonHold);
    ECS_META_COMPONENT(world, ButtonUp);
    ECS_META_COMPONENT(world, ActionButton);
    ECS_META_COMPONENT(world, InputVector);
    ECS_META_COMPONENT(world, DeltaVectorOf);
    ECS_META_COMPONENT(world, ActionVector);
}
