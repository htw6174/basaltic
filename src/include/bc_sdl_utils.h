#ifndef BC_SDL_UTILS_H_INCLUDED
#define BC_SDL_UTILS_H_INCLUDED

#include <SDL2/SDL.h>

static Uint32 bc__baseUserEvent;
#define BC_SDL_REGISTER_USER_EVENTS() bc__baseUserEvent = SDL_RegisterEvents(BC_SDL_EVENTS_COUNT)
#define BC_SDL_USEREVENT_TYPE(event) (bc__baseUserEvent + event)

/**
 * Use these events by first calling BC_SDL_REGISTER_USER_EVENTS once, then wrap with the BC_SDL_USEREVENT_TYPE macro
 * This ensures that the event IDs are unique among any other modules used
 */
typedef enum BC_SDL_Userevents {
    BC_SDL_TILEMOTION = 0,

    BC_SDL_EVENTS_COUNT
} BC_SDL_Userevents;

#endif // BC_SDL_UTILS_H_INCLUDED
