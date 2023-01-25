#ifndef BASALTIC_WINDOW_H_INCLUDED
#define BASALTIC_WINDOW_H_INCLUDED

#include "htw_core.h"
#include "htw_vulkan.h"
#include "ccVector.h"

typedef struct {
    u32 width, height;
    u64 milliSeconds;
    u64 frame;
    // TODO: time of last frame or deltatime?
    htw_VkContext *vkContext;
} bc_WindowContext;

void bc_changeScreenSize(bc_WindowContext *wc, u32 width, u32 height);

#endif // BASALTIC_WINDOW_H_INCLUDED
