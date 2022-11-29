#ifndef BASALTIC_DEFS_H_INCLUDED
#define BASALTIC_DEFS_H_INCLUDED

#include "htw_core.h"

static const u32 bc_chunkSize = 64;

typedef enum {
    BC_APPSTATE_STOPPED,
    BC_APPSTATE_RUNNING,
    BC_APPSTATE_PAUSED,
    BC_APPSTATE_BACKGROUND
} bc_AppState;

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

typedef enum bc_TerrainVisibilityBitFlags {
    BC_TERRAIN_VISIBILITY_GEOMETRY =    0x00000001, // outline of cells and their topography (heightmap value)
    BC_TERRAIN_VISIBILITY_COLOR =       0x00000002, // base color from biome
    BC_TERRAIN_VISIBILITY_MEGALITHS =   0x00000004, // large ruins, cities, and castles. Usually visible whenever geometry is
    BC_TERRAIN_VISIBILITY_VEGETATION =  0x00000008, // presence or absence of trees and shrubs. Usually visible whenever color is
    BC_TERRAIN_VISIBILITY_STRUCTURES =  0x00000010, // roads, towns, and farmhouses in plain view
    BC_TERRAIN_VISIBILITY_MEGAFAUNA =   0x00000020, // huge size creatures or large herds, usually visible from a distance
    BC_TERRAIN_VISIBILITY_CHARACTERS =  0x00000040, // doesn't reveal specifics, just signs of life and activity (e.g. campfire smoke)
    BC_TERRAIN_VISIBILITY_SECRETS =     0x00000080, // hidden cave entrances, lairs, or isolated buildings
    BC_TERRAIN_VISIBILITY_ALL =         0xffffffff,
} bc_TerrainVisibilityBitFlags;

#endif // BASALTIC_DEFS_H_INCLUDED
