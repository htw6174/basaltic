#ifndef KINGDOM_WORLDSTATE_H_INCLUDED
#define KINGDOM_WORLDSTATE_H_INCLUDED

#include "htw_geomap.h"

typedef struct kd_worldState {
    char *name;
    ValueMap *heightMap;
} kd_WorldState;

#endif // KINGDOM_WORLDSTATE_H_INCLUDED
