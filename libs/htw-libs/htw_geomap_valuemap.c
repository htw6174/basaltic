#include <stdio.h>
#include "htw_geomap.h"

htw_ValueMap *htw_geo_createValueMap(u32 width, u32 height, s32 maxValue) {
    int fullSize = sizeof(htw_ValueMap) + (sizeof(int) * width * height);
    htw_ValueMap *newMap = (htw_ValueMap*)malloc(fullSize);
    newMap->width = width;
    newMap->height = height;
    newMap->maxMagnitude = maxValue;
    newMap->values = (int*)(newMap + 1);
    return newMap;
}

s32 htw_geo_getMapValueByIndex(htw_ValueMap *map, u32 cellIndex) {
    return map->values[cellIndex];
}

s32 htw_geo_getMapValue(htw_ValueMap *map, htw_geo_GridCoord cellCoord) {
    return map->values[htw_geo_cellCoordToIndex(cellCoord, map->width)];
}

void htw_geo_setMapValueByIndex(htw_ValueMap *map, u32 cellIndex, s32 value) {
    map->values[cellIndex] = value;
}

void htw_geo_setMapValue(htw_ValueMap *map, htw_geo_GridCoord cellCoord, s32 value) {
    if (cellCoord.x >= map->width || cellCoord.y >= map->height) {
        fprintf(stderr, "coordinates outside map range: %u, %u\n", cellCoord.x, cellCoord.y);
        return;
    }
    map->values[htw_geo_cellCoordToIndex(cellCoord, map->width)] = value;
}

void printValueMap ( htw_ValueMap *map) {
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int value = map->values[(y * map->width) + x];
            printf("%i", value);
        }
        printf("\n");
    }
}
