/* None of the methods here are secure, and should NOT be used for cryptography, passwords, or slot machines
 * If anything, these random number generation methods are optimized for speed and even distribution
 */
#ifndef HTW_RANDOM_H_INCLUDED
#define HTW_RANDOM_H_INCLUDED

#include <math.h>
#include "htw_core.h"

//extern int htw_randRange(int range);

/* Basics */
/**
 * @brief Get a random number in the range [0, range)
 *
 * @param range number of possible results; also equal to max result + 1
 * @return int
 */
static inline int htw_randRange(int range) {
    return rand() % range;
}

/**
 * @brief Roll The Dice; get total and/or individual results for [count] dice rolls. Result for each roll will be in the range [1, sides]
 *
 * @param count number of dice to roll
 * @param sides number of sides on each die
 * @param results NULL or a pointer to an array of at least [count] elements, to record the result of each roll
 * @return sum of all rolls
 */
static int htw_rtd(int count, int sides, int *results) {
    int total = 0;
    if (results == NULL) {
        for (int i = 0; i < count; i++) {
            total += htw_randRange(sides) + 1;
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            int r = htw_randRange(sides) + 1;
            results[i] = r;
            total += r;
        }
    }
    return total;
}

/* Hash functions */
/* xxHash limited implementation for procedural generation
 *
 * Derived from Rune Skovbo Johansen's modified version, read his post on hashing for random numbers here: https://blog.runevision.com/2015/01/primer-on-repeatable-random-numbers.html
 * The following copyright notice applies to everything with a "XXH_*" identifier in this project:
 * Based on integer-optimized implementation Copyright (C) 2015, Rune Skovbo Johansen. (https://bitbucket.org/runevision/random-numbers-testing/)
 * Based on C# implementation Copyright (C) 2014, Seok-Ju, Yun. (https://github.com/noricube/xxHashSharp)
 * Original C Implementation Copyright (C) 2012-2021 Yann Collet (https://code.google.com/p/xxhash/)
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 *
 *
 */

#define XXH_PRIME32_1  0x9E3779B1U  /*!< 0b10011110001101110111100110110001 */
#define XXH_PRIME32_2  0x85EBCA77U  /*!< 0b10000101111010111100101001110111 */
#define XXH_PRIME32_3  0xC2B2AE3DU  /*!< 0b11000010101100101010111000111101 */
#define XXH_PRIME32_4  0x27D4EB2FU  /*!< 0b00100111110101001110101100101111 */
#define XXH_PRIME32_5  0x165667B1U  /*!< 0b00010110010101100110011110110001 */

static u32 xxh_rotateLeft(u32 value, s32 count) {
    return (value << count) | (value >> (32 - count));
}

static inline u32 xxh_bytesToU32(u8 *bytes) {
    u32 value = *bytes;
    value |= (u32)(*(bytes + 1)) << 8;
    value |= (u32)(*(bytes + 2)) << 16;
    value |= (u32)(*(bytes + 3)) << 24;
    return value;
}

static inline u32 xxh_subHash(u32 value, u8 *bytes) {
    u32 word = xxh_bytesToU32(bytes);
    value += word * XXH_PRIME32_2;
    value = xxh_rotateLeft(value, 13);
    value *= XXH_PRIME32_1;
    return value;
}

static u32 xxh_hash2d(u32 seed, u32 x, u32 y) {
    u32 hash = seed + XXH_PRIME32_5;
    hash += 2 * 4; // equivalent to adding input bytecount in the original. Unsure if it's needed here
    // unrolled loop for fixed length input
    // iter 1:
    hash += x * XXH_PRIME32_3;
    hash = xxh_rotateLeft(hash, 17) * XXH_PRIME32_4;
    // iter 2:
    hash += y * XXH_PRIME32_3;
    hash = xxh_rotateLeft(hash, 17) * XXH_PRIME32_4;

    hash ^= hash >> 15;
    hash *= XXH_PRIME32_2;
    hash ^= hash >> 13;
    hash *= XXH_PRIME32_3;
    hash ^= hash >> 16;

    return hash;
}

static u32 xxh_hash(u32 seed, size_t size, u8 *bytes) {
    u32 hash = 0;
    u32 i = 0;

    if (size >= 16) {
        u32 limit = size - 16;
        u32 v1 = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
        u32 v2 = seed + XXH_PRIME32_2;
        u32 v3 = seed + 0;
        u32 v4 = seed + XXH_PRIME32_1;

        do {
            v1 = xxh_subHash(v1, &bytes[i]);
            i += 4;
            v2 = xxh_subHash(v2, &bytes[i]);
            i += 4;
            v3 = xxh_subHash(v3, &bytes[i]);
            i += 4;
            v4 = xxh_subHash(v4, &bytes[i]);
            i += 4;
        } while (i <= limit);

        hash = xxh_rotateLeft(v1, 1) + xxh_rotateLeft(v2, 7) + xxh_rotateLeft(v3, 12) + xxh_rotateLeft(v4, 18);
    } else {
        hash = seed + XXH_PRIME32_5;
    }

    hash += size;

    while (i <= size - 4) {
        u32 word = xxh_bytesToU32(&bytes[i]);
        hash += word * XXH_PRIME32_3;
        hash = xxh_rotateLeft(hash, 17) * XXH_PRIME32_4;
        i += 4;
    }

    while (i < size) {
        hash += bytes[i] * XXH_PRIME32_5;
        hash = xxh_rotateLeft(hash, 11) * XXH_PRIME32_1;
        i++;
    }

    hash ^= hash >> 15;
    hash *= XXH_PRIME32_2;
    hash ^= hash >> 13;
    hash *= XXH_PRIME32_3;
    hash ^= hash >> 16;

    return hash;
}

/* 2D Noise */

// Hermite curve, 3t^2 - 2t^3. Identical to glsl smoothstep in [0, 1]
static float private_htw_smoothCurve(float v) {
    return v * v * (3.0 - 2.0 * v);
}

// Very similar to a Hermite curve, but with continuous derivative when clamped to [0, 1]
static float private_htw_smootherCurve(float v) {
    return v * v * v * (v * (v * 6.0 - 15.0) + 10.0);
}

static float private_htw_surfletCurve(float v) {
    v = 0.5 - (v * v);
    return v * v * v * v * 16.0;
}

// Value noise
static float htw_value2d(u32 seed, float sampleX, float sampleY) {
    float integralX, integralY;
    float fractX = modff(sampleX, &integralX);
    float fractY = modff(sampleY, &integralY);
    u32 x = (u32)integralX, y = (u32)integralY;

    // samples
    float s1 = (float)xxh_hash2d(seed, x, y);
    float s2 = (float)xxh_hash2d(seed, x + 1, y);
    float s3 = (float)xxh_hash2d(seed, x, y + 1);
    float s4 = (float)xxh_hash2d(seed, x + 1, y + 1);

    float l1 = lerp(s1, s2, fractX);
    float l2 = lerp(s3, s4, fractX);
    float l3 = lerp(l1, l2, fractY);

    float normalized = l3 / (float)UINT32_MAX;
    return normalized;
}

static float htw_value2dRepeating(u32 seed, float sampleX, float sampleY, float repeatX, float repeatY) {
    float wrappedX = sampleX - (repeatX * floorf(sampleX / repeatX));
    float wrappedY = sampleY - (repeatY * floorf(sampleY / repeatY));
    return htw_value2d(seed, wrappedX, wrappedY);
}

// Perlin noise
static float htw_perlin2d(u32 seed, float sampleX, float sampleY, u32 octaves) {
    u32 numerator = pow(2, octaves - 1); // used to weight each octave by half the previous octave, halves each iteration
    u32 denominator = pow(2, octaves) - 1;
    float value = 0.0;
    float scaledX = sampleX;
    float scaledY = sampleY;
    for (int i = 0; i < octaves; i++) {
        float weight = (float)numerator / denominator;
        numerator = numerator >> 1;
        value += htw_value2d(seed, scaledX, scaledY) * weight;
        scaledX *= 2.0;
        scaledY *= 2.0;
    }
    return value;
}

static float htw_perlin2dRepeating(u32 seed, float sampleX, float sampleY, u32 octaves, float repeatX, float repeatY) {
    u32 numerator = pow(2, octaves - 1); // used to weight each octave by half the previous octave, halves each iteration
    u32 denominator = pow(2, octaves) - 1;
    float value = 0.0;
    float scaledX = sampleX;
    float scaledY = sampleY;
    for (int i = 0; i < octaves; i++) {
        float weight = (float)numerator / denominator;
        numerator = numerator >> 1;
        value += htw_value2dRepeating(seed, scaledX, scaledY, repeatX, repeatY) * weight;
        scaledX *= 2.0;
        scaledY *= 2.0;
    }
    return value;
}

/**
 * @brief Very limited simplex noise implementation. Designed for use on a hexagonal tile map with descrete cells, so smoothness/continuous derivites are not a concern, and the input sample coordinates are assumed to already be in a skewed hexmap coordinate space.
 *
 * @param seed p_seed:...
 * @param sampleX p_sampleX:...
 * @param sampleY p_sampleY:...
 * @return float
 */
static float htw_simplex2d(u32 seed, float sampleX, float sampleY, u32 repeatX, u32 repeatY) {
    float integralX, integralY;
    float fractX = modff(sampleX, &integralX);
    float fractY = modff(sampleY, &integralY);
    u32 x = (u32)integralX, y = (u32)integralY;

    // The skewing in my system is different than a typical simplex noise impelmentation (angle between x and y axis is 60 instead of 120), so determing which simplex a sample lies in is fractX + fractY < 1, instead of x > y
    u32 simplex = fractX + fractY < 1 ? 0 : 1;

    // wrap sample coordinates
    u32 x0 = x % repeatX;
    u32 x1 = (x + 1) % repeatX;
    u32 y0 = y % repeatY;
    u32 y1 = (y + 1) % repeatY;

    // kernels - deterministic random values at closest simplex corners
    float k1 = (float)xxh_hash2d(seed, x1, y0) / (float)UINT32_MAX;
    float k2 = (float)xxh_hash2d(seed, x0, y1) / (float)UINT32_MAX;
    u32 k3raw = simplex == 0 ? xxh_hash2d(seed, x0, y0) : xxh_hash2d(seed, x1, y1);
    float k3 = (float)k3raw / (float)UINT32_MAX;

    // distance of sample from each corner, using cube coordinate distance
    // = (abs(x1 - x2) + abs(x1 + y1 - x2 - y2) + abs(y1 - y2)) / 2
    float d1 = (fabs(fractX - 1.0) + fabs(fractX + fractY - 1.0 - 0.0) + fabs(fractY - 0.0)) / 2.0;
    float d2 = (fabs(fractX - 0.0) + fabs(fractX + fractY - 0.0 - 1.0) + fabs(fractY - 1.0)) / 2.0;
    float d3 = (fabs(fractX - simplex) + fabs(fractX + fractY - simplex - simplex) + fabs(fractY - simplex)) / 2.0;

    // TODO: could benefit from a non-linear surflet dropoff function or other smoothing, but adding more octaves is good enough for now
    // remap and smooth distance
    // d1 = private_htw_smoothCurve(fmin(d1, 1.0));
    // d2 = private_htw_smoothCurve(fmin(d2, 1.0));
    // d3 = private_htw_smoothCurve(fmin(d3, 1.0));

    // d1 = private_htw_surfletCurve(d1);
    // d2 = private_htw_surfletCurve(d2);
    // d3 = private_htw_surfletCurve(d3);

    // kernel contribution from each corner
    float c1 = k1 * (1.0 - d1);
    float c2 = k2 * (1.0 - d2);
    float c3 = k3 * (1.0 - d3);

    float sum = (c1 + c2 + c3);

    return sum;
}

static float htw_simplex2dLayered(u32 seed, float sampleX, float sampleY, u32 repeat, u32 layers) {
    u32 numerator = pow(2, layers - 1); // used to weight each layer by half the previous layer, halves each iteration
    u32 denominator = pow(2, layers) - 1;
    float value = 0.0;
    float scaledX = sampleX;
    float scaledY = sampleY;
    for (int i = 0; i < layers; i++) {
        float weight = (float)numerator / denominator;
        numerator = numerator >> 1;
        value += htw_simplex2d(seed, scaledX, scaledY, repeat, repeat) * weight;
        scaledX *= 2.0;
        scaledY *= 2.0;
        repeat *= 2;
    }
    return value;
}

#endif // HTW_RANDOM_H_INCLUDED
