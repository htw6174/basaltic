
#include "htw_core.h"
#include "htw_vulkan.h"
#include "basaltic_window.h"

static htw_VkContext *createWindow(u32 width, u32 height);

bc_WindowContext *bc_createWindow(u32 width, u32 height) {
    bc_WindowContext *wc = malloc(sizeof(bc_WindowContext));
    wc->width = width;
    wc->height = height;
    wc->vkContext = createWindow(width, height);
    wc->frame = 0;

    return wc;
}

void bc_destroyGraphics(bc_WindowContext *wc) {
    htw_destroyVkContext(wc->vkContext);
}

void bc_changeScreenSize(bc_WindowContext *wc, u32 width, u32 height) {
    // TODO
}

htw_VkContext *createWindow(u32 width, u32 height) {
    SDL_Window *sdlWindow = SDL_CreateWindow("basaltic", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, HTW_VK_WINDOW_FLAGS);
    htw_VkContext *context = htw_createVkContext(sdlWindow);
    return context;
}
