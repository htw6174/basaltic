#ifndef BASALTIC_DEFS_H_INCLUDED
#define BASALTIC_DEFS_H_INCLUDED

#include "htw_core.h"

// TODO: move most of these defs into the View or Model modules

typedef enum {
    BC_SUPERVISOR_SIGNAL_NONE,
    BC_SUPERVISOR_SIGNAL_START_MODEL,
    BC_SUPERVISOR_SIGNAL_STOP_MODEL,
    BC_SUPERVISOR_SIGNAL_RESTART_MODEL,
    BC_SUPERVISOR_SIGNAL_STOP_PROCESS,
} bc_SupervisorSignal;

typedef enum {
    BC_PROCESS_STATE_STOPPED,
    BC_PROCESS_STATE_RUNNING,
    BC_PROCESS_STATE_PAUSED,
    BC_PROCESS_STATE_BACKGROUND
} bc_ProcessState;

typedef enum bc_StartupMode {
    BC_STARTUP_MODE_MAINMENU,
    BC_STARTUP_MODE_NEWGAME,
    BC_STARTUP_MODE_LOADGAME,
    BC_STARTUP_MODE_CONTINUEGAME
} bc_StartupMode;

typedef enum bc_InterfaceMode {
    BC_INTERFACE_MODE_SYSTEM_MENU,
    BC_INTERFACE_MODE_GAMEPLAY,
    BC_INTERFACE_MODE_EDITOR
} bc_InterfaceMode;

#endif // BASALTIC_DEFS_H_INCLUDED