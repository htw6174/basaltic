#include <math.h>
#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"

void htw_geo_fillUniform(htw_ValueMap *map, s32 uniformValue) {
    u32 mapLength = map->width * map->height;
    for (u32 i = 0; i < mapLength; i++) {
        htw_geo_setMapValueByIndex(map, i, uniformValue);
    }
}

void htw_geo_fillChecker(htw_ValueMap *map, s32 value1, s32 value2, u32 gridOrder) {
    u32 gridBit = 1 << (gridOrder - 1);
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            u32 cx = x & gridBit;
            u32 cy = y & gridBit;
            if (cx ^ cy) {
                htw_geo_setMapValue(map, (htw_geo_GridCoord){x, y}, value1);
            } else {
                htw_geo_setMapValue(map, (htw_geo_GridCoord){x, y}, value2);
            }
        }
    }
}

void htw_geo_fillGradient(htw_ValueMap *map, int gradStart, int gradEnd ) {
    gradStart = min_int( gradStart, map->maxMagnitude);
    gradEnd = min_int( gradEnd, map->maxMagnitude);
    for (int y = 0; y < map->height; y++) {
        int gradCurrent = lerp_int( gradStart, gradEnd, (double)y / map->height );
        for (int x = 0; x < map->width; x++) {
            htw_geo_setMapValue(map, (htw_geo_GridCoord){x, y}, gradCurrent);
        }
    }
}

void htw_geo_fillCircularGradient(htw_ValueMap* map, htw_geo_GridCoord center, s32 gradStart, s32 gradEnd, float radius) {
    gradStart = min_int(gradStart, map->maxMagnitude);
    gradEnd = min_int(gradEnd, map->maxMagnitude);
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            float distance = htw_geo_hexGridDistance(center, (htw_geo_GridCoord){x, y});
            int gradValue = 0;
            if (distance < radius) {
                gradValue = lerp_int(gradEnd, gradStart, (radius - distance) / radius);
            }
            htw_geo_setMapValue(map, (htw_geo_GridCoord){x, y}, gradValue);
        }
    }
}

void htw_geo_fillNoise(htw_ValueMap* map, u32 seed) {
    for (u32 y = 0; y < map->height; y++) {
        for (u32 x = 0; x < map->width; x++) {
            u32 val = xxh_hash2d(seed, x, y);
            val = val % map->maxMagnitude;
            htw_geo_setMapValue(map, (htw_geo_GridCoord){x, y}, val);
        }
    }
}

void htw_geo_fillSmoothNoise(htw_ValueMap* map, u32 seed, float scale) {
    for (u32 y = 0; y < map->height; y++) {
        for (u32 x = 0; x < map->width; x++) {
            float scaledX, scaledY;
            htw_geo_getHexCellPositionSkewed((htw_geo_GridCoord){x, y}, &scaledX, &scaledY);
            scaledX *= scale;
            scaledY *= scale;
            float val = htw_value2d(seed, scaledX, scaledY);
            val = floorf(val * map->maxMagnitude);
            htw_geo_setMapValue(map, (htw_geo_GridCoord){x, y}, val);
        }
    }
}

void htw_geo_fillPerlin(htw_ValueMap* map, u32 seed, u32 octaves, s32 posX, s32 posY, float scale, float repeatX, float repeatY) {
    for (u32 y = 0; y < map->height; y++) {
        for (u32 x = 0; x < map->width; x++) {
            float scaledX, scaledY;
            htw_geo_getHexCellPositionSkewed((htw_geo_GridCoord){x + posX, y + posY}, &scaledX, &scaledY);
            scaledX *= scale;
            scaledY *= scale;
            float val = htw_perlin2dRepeating(seed, scaledX, scaledY, octaves, repeatX, repeatY);
            val = floorf(val * map->maxMagnitude);
            htw_geo_setMapValue(map, (htw_geo_GridCoord){x, y}, val);
        }
    }
}

void htw_geo_fillSimplex(htw_ValueMap* map, u32 seed, u32 octaves, s32 posX, s32 posY, u32 repeatInterval, u32 samplesPerRepeat) {
    float scale = (float)samplesPerRepeat / repeatInterval;
    for (u32 y = 0; y < map->height; y++) {
        for (u32 x = 0; x < map->width; x++) {
            float scaledX = (x + posX) * scale;
            float scaledY = (y + posY) * scale;
            float val = htw_simplex2dLayered(seed, scaledX, scaledY, samplesPerRepeat, octaves);
            val = floorf(val * map->maxMagnitude);
            htw_geo_setMapValue(map, (htw_geo_GridCoord){x, y}, val);
        }
    }
}

s32 htw_geo_circularGradientByGridCoord(htw_ChunkMap *chunkMap, htw_geo_GridCoord cellCoord, htw_geo_GridCoord center, s32 gradStart, s32 gradEnd, float radius) {
    float distance = htw_geo_hexCartesianDistance(chunkMap, cellCoord, center);
    s32 gradValue = 0;
    if (distance < radius) {
        gradValue = lerp_int(gradEnd, gradStart, (radius - distance) / radius);
    }
    return gradValue;
}

float htw_geo_simplex(htw_ChunkMap *chunkMap, htw_geo_GridCoord cellCoord, u32 seed, u32 octaves, u32 samplesPerRepeat) {
    float scale = (float)samplesPerRepeat / chunkMap->mapWidth;
    float scaledX = cellCoord.x * scale;
    float scaledY = cellCoord.y * scale;
    return htw_simplex2dLayered(seed, scaledX, scaledY, samplesPerRepeat, octaves);
}
