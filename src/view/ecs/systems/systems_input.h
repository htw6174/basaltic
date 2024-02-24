#ifndef SYSTEMS_INPUT_H_INCLUDED
#define SYSTEMS_INPUT_H_INCLUDED

#include "flecs.h"

void SystemsInputImport(ecs_world_t *world);

/**
 * @brief If you have an existing event polling system or want to prefilter incoming events, disable the ProcessEvents system and pass individual events in here
 *
 * @param world p_world:...
 * @param e pointer to backend-dependent event type. e.g. for SDL, this should be SDL_Event*
 * @return return True if the event was matched to any binding
 */
bool SystemsInput_ProcessEvent(ecs_world_t *world, const void *e);

#endif // SYSTEMS_INPUT_H_INCLUDED
