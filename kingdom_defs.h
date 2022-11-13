#ifndef KINGDOM_DEFS_H_INCLUDED
#define KINGDOM_DEFS_H_INCLUDED


typedef enum {
    KD_APPSTATE_STOPPED,
    KD_APPSTATE_RUNNING,
    KD_APPSTATE_PAUSED,
    KD_APPSTATE_BACKGROUND
} KD_APPSTATE;

typedef enum kd_TerrainVisibilityBitFlags {
    KD_TERRAIN_VISIBILITY_GEOMETRY =    0x00000001, // outline of cells and their topography (heightmap value)
    KD_TERRAIN_VISIBILITY_COLOR =       0x00000002, // base color from biome
    KD_TERRAIN_VISIBILITY_MEGALITHS =   0x00000004, // large ruins, cities, and castles. Usually visible whenever geometry is
    KD_TERRAIN_VISIBILITY_VEGETATION =  0x00000008, // presence or absence of trees and shrubs. Usually visible whenever color is
    KD_TERRAIN_VISIBILITY_STRUCTURES =  0x00000010, // roads, towns, and farmhouses in plain view
    KD_TERRAIN_VISIBILITY_MEGAFAUNA =   0x00000020, // huge size creatures or large herds, usually visible from a distance
    KD_TERRAIN_VISIBILITY_CHARACTERS =  0x00000040, // doesn't reveal specifics, just signs of life and activity (e.g. campfire smoke)
    KD_TERRAIN_VISIBILITY_SECRETS =     0x00000080, // hidden cave entrances, lairs, or isolated buildings
} kd_TerrainVisibilityBitFlags;

#endif // KINGDOM_DEFS_H_INCLUDED
