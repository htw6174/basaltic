#version 300 es
precision mediump float;
precision highp isampler2D;

//#include "uniforms.h"

// Left-hand neighbor in xy, right-hand neighbor in zw
ivec4 sampleOffsets[6] = ivec4[](
    // Begins from samples used for north vertex, in clockwise order
    ivec4(-1, 1, 0, 1),
    ivec4(0, 1, 1, 0),
    ivec4(1, 0, 1, -1),
    ivec4(1, -1, 0, -1),
    ivec4(0, -1, -1, 0),
    ivec4(-1, 0, -1, 1)
);

const float hexStepY = sqrt(0.75); // Y distance between cell rows
const float hexStepX = 1.0; // X distance between cells in the same row
const float hexStepXY = 0.5; // X distance from a cell to its neighbor 1 row up or down

// Left-hand neighbor in xy, right-hand neighbor in zw
vec4 neighborPositions[6] = vec4[](
    // Begins from samples used for north vertex, in clockwise order
    vec4(-hexStepXY, hexStepY, hexStepXY, hexStepY),
    vec4(hexStepXY, hexStepY, hexStepX, 0.0),
    vec4(hexStepX, 0.0, hexStepXY, -hexStepY),
    vec4(hexStepXY, -hexStepY, -hexStepXY, -hexStepY),
    vec4(-hexStepXY, -hexStepY, -hexStepX, 0.0),
    vec4(-hexStepXY, 0.0, -hexStepXY, hexStepY)
);

uniform mat4 pv;
uniform mat4 m;

uniform isampler2D terrain;

// per-instance
layout(location = 0) in vec4 in_chunkPosition; // NOTE: this has a 1.0 w component built in. Remember not to add to other w components!
layout(location = 1) in vec2 in_rootCoord;
// per-vertex
layout(location = 2) in vec2 in_position;
layout(location = 3) in float in_neighborWeight; // integral part is index into sampleOffsets, fractional part is weight from left to right sample
layout(location = 4) in vec3 in_barycentric;
layout(location = 5) in vec2 in_localCoord; // For compatability, this is an s16 normalized to a float. Multiply by 2e16 to restore.

vec3 remapBarycentric(vec3 barycentric, float yFactor, float zFactor, float inset) {
    // Moving in affine space can be considered to be with respect to one coordinate (x), and will cause a change in the other 2 coordinates (y & z) whose sum is equal to the change in x. The ratio of y change to z change [0..1] tells the direction of motion: in cartesian space, within 30 degrees to either side of the x axis.
    // Find remapping difference for y and z seperately. x delta is their sum
    float yRange = 1.0 - barycentric.z; // range limited by invariant axis; must also compress back to this range after smoothstep
    float yRemapped = mix(barycentric.y, smoothstep(inset, yRange - inset, barycentric.y) * yRange, yFactor);

    float zRange = 1.0 - barycentric.y;
    float zRemapped = mix(barycentric.z, smoothstep(inset, zRange - inset, barycentric.z) * zRange, zFactor);

    float xDelta = (barycentric.y - yRemapped) + (barycentric.z - zRemapped);
    return vec3(barycentric.x + xDelta, yRemapped, zRemapped);
}

ivec4 terrainFetch(ivec2 coord) {
    // texelFetch is undefined on out of bounds access. Must manually wrap first
    ivec2 wrap = textureSize(terrain, 0);
    return texelFetch(terrain, (coord + wrap) % wrap, 0);
}

// clumsy replacement for glsl 4.0's bitfieldExtract
int bitfieldExtract(in int value, in int offset, in int bits) {
    value = value >> offset;
    int mask = (1 << bits) - 1;
    value = value & mask;
    int sign_bit = value >> (bits - 1);
    int high = ~mask;
    // if sign bit is 1, set all high bits to 1
    int sign_high = high * sign_bit;
    value = value | sign_high;
    return value;
}

float interpolate_height(vec3 barycentric, ivec2 cellCoord, int neighborhood) {
    ivec4 cd = terrainFetch(cellCoord);
    ivec4 offsets = sampleOffsets[neighborhood];
    ivec4 cdl = terrainFetch(cellCoord + ivec2(offsets.x, offsets.y));
    ivec4 cdr = terrainFetch(cellCoord + ivec2(offsets.z, offsets.w));

    int h1 = bitfieldExtract(cd.r, 0, 8);
    int h2 = bitfieldExtract(cdl.r, 0, 8);
    int h3 = bitfieldExtract(cdr.r, 0, 8);

    // create sharp cliffs
    // warp barycentric space along one axis if slope in that direction is high enough
    int slopeLeft = h2 - h1;
    int slopeRight = h3 - h1;
    float remapLeft = abs(slopeLeft) > 3 ? 1.0 : 0.0;
    float remapRight = abs(slopeRight) > 3 ? 1.0 : 0.0;

    float slopeFactor = 0.2;

    // must remap after adding planck for accurate slope
    barycentric = remapBarycentric(barycentric, remapLeft, remapRight, slopeFactor);

    // Use barycentric coord to interpolate samples
    float elev1 = (float(h1) * barycentric.x) + (float(h2) * barycentric.y) + (float(h3) * barycentric.z);

    return elev1;
}

void main()
{
    // unpack terrain data
    ivec2 cellCoord = ivec2(in_rootCoord) + ivec2(in_localCoord * 32767.0); // scale normalized s16 back to full range. see also: unpackSnorm2x16

    // get neighboring cell data
    int neighborhood = int(floor(in_neighborWeight));

    float elevation = interpolate_height(in_barycentric, cellCoord, neighborhood);

    vec3 unscaledPosition = vec3(in_position, elevation);
    vec3 localPosition = unscaledPosition * vec3(1.0, 1.0, 0.1);
    vec4 worldPosition = m * (in_chunkPosition + vec4(localPosition, 0.0));

    gl_Position = pv * worldPosition;
}

