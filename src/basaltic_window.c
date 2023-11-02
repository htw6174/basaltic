#include "basaltic_window.h"
#include "htw_core.h"
#include "SDL2/SDL.h"
#ifdef _WIN32
#include <GL/glew.h>
#endif

static SDL_Window *createWindow(u32 width, u32 height);

bc_WindowContext bc_createWindow(u32 width, u32 height) {
    bc_WindowContext wc = {
        .width = width,
        .height = height,
        .window = createWindow(width, height),
        .frame = 0,
        .performanceFrequency = SDL_GetPerformanceFrequency(),
    };

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#ifdef __EMSCRIPTEN__
    printf("Creating SDL GLES3 context...\n");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    printf("Creating SDL OpenGL 33 context...\n");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_GLContext glc = SDL_GL_CreateContext(wc.window);
    if (glc == NULL) {
        printf("Failed to create GL Context: %s\n", SDL_GetError());
    }
    SDL_GL_SetSwapInterval(1); // enable vsync for framerate limiting

    if (SDL_GL_MakeCurrent(wc.window, glc) != 0) {
        printf("Failed to activate GL Context: %s\n", SDL_GetError());
    }
    wc.glContext = glc;

#ifdef _WIN32
    GLenum error = glewInit();
    if (error != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(error));
    }
#endif

    return wc;
}

void bc_destroyWindow(bc_WindowContext *wc) {
    SDL_GL_DeleteContext(wc->glContext);
    SDL_DestroyWindow(wc->window);
}

void bc_changeScreenSize(bc_WindowContext *wc, u32 width, u32 height) {
    wc->width = width;
    wc->height = height;
}

SDL_Window *createWindow(u32 width, u32 height) {
    SDL_Window *sdlWindow = SDL_CreateWindow("basaltic", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_OPENGL);
    if (sdlWindow == NULL) {
        printf("Failed to create SDL window: %s\n", SDL_GetError());
    }
    return sdlWindow;
}
