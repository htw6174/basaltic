#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "htw_core.h"
#include "htw_geomap.h"

void testMap() {
    TileMap *myTMap = createTileMap (15, 5);
    setMapTile ( myTMap, ( MapTile ){5, 0}, 14, 4);
    printTileMap ( myTMap );

    putchar('\n');

    ValueMap *myVMap = createValueMap (20, 6, 9);
    fillGradient(myVMap, 10, 0);
    printValueMap(myVMap);
}

Uint32 *valueMapToBitmap(ValueMap *map, Uint32 format) {
    int mapSize = map->width * map->height;
    Uint32 *bmp = malloc(mapSize * sizeof(Uint32));
    for (int i = 0; i < mapSize; i++) {
        int value = map->values[i];
        value = remap_int(0, map->maxValue, 0, 255, value);
        SDL_PixelFormat *pixelFormat = SDL_AllocFormat(format);
        bmp[i] = SDL_MapRGB(pixelFormat, value, 0, 0);
    }
    return bmp;
}

int main(int argc, char *argv[])
{
    //loadTileDefinitions ("resources/cell_types");

    int running = 1;
    SDL_Window *window = NULL;
    SDL_Surface *surface = NULL;
    SDL_Event e;

    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

    window = SDL_CreateWindow("kingdom", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    surface = SDL_GetWindowSurface(window);

    // Paint some terrain
    ValueMap *myVMap = createValueMap(20, 10, 255);
    fillGradient(myVMap, 0, 255);
    Uint32 terrainFormat = SDL_PIXELFORMAT_RGBA32;
    //Uint32 *bmp = valueMapToBitmap(myVMap, terrainFormat);
    SDL_Surface *terrain = NULL;
    //terrain = SDL_CreateRGBSurfaceWithFormatFrom(bmp, myVMap->width, myVMap->height, sizeof(Uint32) / 8, myVMap->width * sizeof(Uint32), terrainFormat);

    while (running) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT)
                running = 0;
        }

        //SDL_BlitSurface(terrain, NULL, surface, NULL);

        SDL_UpdateWindowSurface(window);
    }

    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}
