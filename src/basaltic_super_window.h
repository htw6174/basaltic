#ifndef BASALTIC_SUPER_WINDOW_H_INCLUDED
#define BASALTIC_SUPER_WINDOW_H_INCLUDED

#include "htw_core.h"
#include "basaltic_window.h"

bc_WindowContext bc_createWindow(u32 width, u32 height);
void bc_destroyWindow(bc_WindowContext *wc);

#endif // BASALTIC_SUPER_WINDOW_H_INCLUDED
