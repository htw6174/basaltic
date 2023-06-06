#ifndef BASALTIC_WINDOW_H_INCLUDED
#define BASALTIC_WINDOW_H_INCLUDED

#include "htw_core.h"
#include "SDL2/SDL.h"

typedef struct {
    u32 width, height;
    u64 milliSeconds; // Milliseconds since program start
    u64 frame; // Number of frames since program start
    u64 lastFrameDuration; // In milliseconds
    SDL_Window *window;
    SDL_GLContext glContext;
} bc_WindowContext;

void bc_changeScreenSize(bc_WindowContext *wc, u32 width, u32 height);

#endif // BASALTIC_WINDOW_H_INCLUDED
