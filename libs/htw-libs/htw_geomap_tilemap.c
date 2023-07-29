#include <stdio.h>
#include <math.h>
#include "htw_geomap.h"


htw_TileMap *createTileMap (u32 width, u32 height) {
    int fullSize = sizeof(htw_TileMap) + (sizeof( htw_geo_MapTile ) * width * height);
    htw_TileMap *newMap = ( htw_TileMap*)malloc(fullSize);
    newMap->width = width;
    newMap->height = height;
    newMap->tiles = (htw_geo_MapTile *)(newMap + 1);
    return newMap;
}

htw_geo_MapTile *getMapTile ( htw_TileMap *map, int x, int y) {
    return &map->tiles[(y * map->width) + x];
}

void setMapTile ( htw_TileMap *map, htw_geo_MapTile tile, int x, int y) {
    *getMapTile (map, x, y) = tile;
}

void printCell(void *val) {
    printf("%i", *(int*)val);
}

void printTileMap ( htw_TileMap *map) {
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int value = map->tiles[(y * map->width) + x].id;
            printf("%i", value);
        }
        printf("\n");
    }
}
