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
uniform int visibilityOverride;
uniform mat4 m;

uniform isampler2D terrain;

// per-instance
layout(location = 0) in vec4 in_chunkPosition;
layout(location = 1) in vec2 in_rootCoord;
// per-vertex
layout(location = 2) in vec2 in_position;
// Used to find the uv coords of nearest cells to sample. Integral part is index into sampleOffsets, fractional part is weight from left to right sample
layout(location = 3) in float in_neighborWeight;
// position of vertex with respect to nearest cells. x: 'home' cell for vertex, y: 'left-hand' cell, z: 'right-hand' cell
// will always be in the corner nearest 'home', so barycentric.x >= barycentric.y, and barycentric.x >= barycentric.z
layout(location = 4) in vec3 in_barycentric;
layout(location = 5) in vec2 in_localCoord; // For compatability, this is an s16 normalized to a float. Multiply by 2e16 to restore.

out vec4 inout_data1;
out vec4 inout_data2;
out vec3 inout_pos;
out vec3 inout_normal;
out float inout_radius;
flat out ivec2 inout_cellCoord;
//flat out uint inout_cellIndex;

float rand(float x) {
    return fract(sin(x) * 43758.5453123);
}

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

// returns barycentric corner of lowest contributing cell
// remember, all this is actually doing is moving a point in the "barycentric triangle" from one place to another. In this case, usually moving from the center or middle of an edge to one of the corners (or at least towards that corner)
// - so for the simplest possible version of this, I just want to return one of 3 unit vectors
// - to make this a smooth function, need to change behavior if slope is negative:
//   - smooth transition from b.x = 1 at deadzone to original position at edge
// ISSUE: low edge of smoothstep needs to be 1/2 at edge centers and 1/3 at corner points
// - more specifically, where the edge of smoothstep between 2 corners is depends on value of 3rd corner: 0.5 at c=0, 0.33.. at c=0.33.., 0.25 at c=0.5, ... => (1-c) / 2
// high edge of smoothstep also needs to change based on 3rd corner and deadzone.
// - For deadzone 0.25: 0.75 at c=0, ~0.6 at c=0.25, 0.5 at c=0.5
vec3 remapBarycentricBySlope(vec3 barycentric, float deadzone, float slopeY, float slopeZ) {
    vec3 remapped;

    float b = slopeY > 0.0 ? barycentric.y : barycentric.x; // x: 0.5 edge, 0.33 center
    float bLowEdge = (1.0 - barycentric.z) / 2.0; // 0.5 edge, 0.33 center
    float bHighEdge = 1.0 - (deadzone + barycentric.z / 2.0); // 0.8 edge, 0.684 center
    b = smoothstep(bLowEdge, bHighEdge, b);// * yRange; // 0.0 edge, 0.0 center
    b = slopeY > 0.0 ? b : 1.0 - b; // 1.0 edge, 1.0 center

    float c = slopeZ > 0.0 ? barycentric.z : barycentric.x;
    float cLowEdge = (1.0 - barycentric.y) / 2.0;
    float cHighEdge = 1.0 - (deadzone + barycentric.y / 2.0);
    c = smoothstep(cLowEdge, cHighEdge, c);// * zRange;
    c = slopeZ > 0.0 ? c : 1.0 - c;

    // slope going from b.y to b.z;
    float slopeW = slopeZ - slopeY;
    float d = slopeW > 0.0 ? barycentric.z : barycentric.y;
    float dLowEdge = (1.0 - barycentric.x) / 2.0;
    float dHighEdge = 1.0 - (deadzone + barycentric.x / 2.0);
    d = smoothstep(dLowEdge, dHighEdge, d);
    d = slopeW > 0.0 ? d : 1.0 - d;

    float yRange = (1.0 - barycentric.z);
    float zRange = (1.0 - barycentric.y);

    float scaleDown = b + c > 1.0 ? 1.0 / (b + c) : 1.0;

    remapped.y = b * scaleDown; // min(barycentric.y, b); // * yRange;
    remapped.z = c * scaleDown; //min(barycentric.z, c); // * zRange;

    //remapped.x = 1.0 - clamp(remapped.y + remapped.z, 0.0, 1.0); // 1 - 2 = -1
    remapped.x = 1.0 - (remapped.y + remapped.z);

    return remapped;
}

vec3 expandBarycentric(vec3 barycentric, float radius) {
    float x = step(radius, barycentric.x);
    float diff = barycentric.x - x;
    float y = barycentric.y - (diff * 0.5);
    float z = barycentric.z - (diff * 0.5);
    return vec3(x, y, z);
}

ivec4 terrainFetch(ivec2 coord) {
    // texelFetch is undefined on out of bounds access. Must manually wrap first
    ivec2 wrap = textureSize(terrain, 0);
    return texelFetch(terrain, (coord + wrap) % wrap, 0);
}

//clumsy replacement for glsl 4.0's bitfieldExtract
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

uint bitfieldExtractU(in int value, in int offset, in int bits) {
    value = value >> offset;
    int mask = (1 << bits) - 1;
    value = value & mask;
    return uint(value);
}

float interpolate_flat(float here, float left, float right, vec3 barycentric) {
    return (here * barycentric.x) + (left * barycentric.y) + (right * barycentric.z);
}

float interpolate_expand(float here, float left, float right, vec3 barycentric, float threshold) {
    barycentric = expandBarycentric(barycentric, 0.5);
    return (here * barycentric.x) + (left * barycentric.y) + (right * barycentric.z);
}

// TODO: want to change weights so that the lowest elevation cell is always favored.
// For a cell with h1 = 0, h2 (left cell) = 1, h3 (right cell) = -1:
// - center (barycentric.x == 1) always == h1
// - vertex on left edge (barycentric.x == barycentric.y) == h1
// - vertex on right edge (barycentric.x == barycentric.z) == h3
// - vertex between center and right edge == smooth transition from h1 to h3
float interpolate_height(int h1, int h2, int h3, vec3 barycentric, int neighborhood) {
    // create sharp cliffs
    // warp barycentric space along one axis if slope in that direction is high enough
    int slopeLeft = h2 - h1;
    int slopeRight = h3 - h1;
    float remapLeft = abs(slopeLeft) > 3 ? 1.0 : 0.0;
    float remapRight = abs(slopeRight) > 3 ? 1.0 : 0.0;
    //float remapLeft = slopeLeft >= 0 ? 1.0 : 0.0;
    //float remapRight = slopeRight >= 0 ? 1.0 : 0.0;

    float deadzone = 0.22;

    // approximate local derivative
    // create 2 more barycentric coords by moving in both directions away from home cell
    const float planck = 0.0001;
    // Right-hand rule for cross product results - index finger, towards cdr along line parallel to cd -> cdr
    vec3 bary1 = vec3(barycentric.x - planck, barycentric.y, barycentric.z + planck);
    // middle finger, towards cdl along line parallel to cd -> cdl
    vec3 bary2 = vec3(barycentric.x - planck, barycentric.y + planck, barycentric.z);

    // TEST: favor lowest cell
    //barycentric = remapBarycentricBySlope(barycentric, deadzone, float(slopeLeft), float(slopeRight));
    //bary1 = remapBarycentricBySlope(bary1, deadzone, float(slopeLeft), float(slopeRight));
    //bary2 = remapBarycentricBySlope(bary2, deadzone, float(slopeLeft), float(slopeRight));

    // must remap after adding planck for correct slope
    barycentric = remapBarycentric(barycentric, remapLeft, remapRight, deadzone);
    bary1 = remapBarycentric(bary1, remapLeft, remapRight, deadzone);
    bary2 = remapBarycentric(bary2, remapLeft, remapRight, deadzone);

    // Use barycentric coord to interpolate samples
    // explicit conversion for GLES
    float fh1 = float(h1);
    float fh2 = float(h2);
    float fh3 = float(h3);
    float elev1 = (fh1 * barycentric.x) + (fh2 * barycentric.y) + (fh3 * barycentric.z);
    float elev2 = (fh1 * bary1.x) + (fh2 * bary1.y) + (fh3 * bary1.z);
    float elev3 = (fh1 * bary2.x) + (fh2 * bary2.y) + (fh3 * bary2.z);

    // get sample positions as cartesian coordinates
    // in the special case of an equilateral triangle, this is easy to find. cartesian distance == (barycentric distance * grid scale)
    // still need to rotate vectors according to sample cell's direction; == -(neighborhood - [1, 2]) * 60 degrees
    // More general formula : https://www.iue.tuwien.ac.at/phd/nentchev/node26.html#eq:lambda_representation_2D_xy
    float rot1 = -float(neighborhood - 1) * radians(60.0);
    float rot2 = -float(neighborhood - 2) * radians(60.0);
    // Get distance from vertex
    vec3 tangent = vec3(cos(rot1) * planck, sin(rot1) * planck, (elev2 - elev1) * 0.1); // TODO: provide terrain scale as uniform, multiply in
    vec3 bitangent = vec3(cos(rot2) * planck, sin(rot2) * planck, (elev3 - elev1) * 0.1);

    // approximate vertex normal from cross product
    inout_normal = normalize(cross(tangent, bitangent));

    return elev1;
}

float extract_interpolate(int pack, int pack_left, int pack_right, vec3 barycentric, int offset, int bits) {
    float here = float(bitfieldExtractU(pack, offset, bits));
    float left = float(bitfieldExtractU(pack_left, offset, bits));
    float right = float(bitfieldExtractU(pack_right, offset, bits));
    return interpolate_flat(here, left, right, barycentric);
}

float extract_normalize_interpolate(int pack, int pack_left, int pack_right, vec3 barycentric, int offset, int bits) {
    float here = float(bitfieldExtractU(pack, offset, bits)) / 255.0;
    float left = float(bitfieldExtractU(pack_left, offset, bits)) / 255.0;
    float right = float(bitfieldExtractU(pack_right, offset, bits)) / 255.0;
    return interpolate_flat(here, left, right, barycentric);
}

float extract_normalize_interpolate_expand(int pack, int pack_left, int pack_right, vec3 barycentric, float radius, int offset, int bits) {
    // TODO: move outside, make more configurable, rename param
    radius = max(0.0, radius * 2.0 - 1.0);
    float here = float(bitfieldExtractU(pack, offset, bits)) / 255.0;
    float left = float(bitfieldExtractU(pack_left, offset, bits)) / 255.0;
    float right = float(bitfieldExtractU(pack_right, offset, bits)) / 255.0;
    float interpolated = interpolate_flat(here, left, right, barycentric);
    return mix(here, interpolated, radius);
}

void main()
{
    // unpack terrain data
    inout_cellCoord = ivec2(in_rootCoord) + ivec2(in_localCoord * 32767.0); // scale normalized s16 back to full range. see also: unpackSnorm2x16

    // 0.75 at center of edge, 1.0 at corners
    //inout_radius = (1.0 - in_barycentric.x) * 1.5;

    // 0 at center, 1.0 at corners and edges
    inout_radius = 1.0 - (in_barycentric.x - max(in_barycentric.y, in_barycentric.z));

    // get neighboring cell data
    int neighborhood = int(floor(in_neighborWeight));
    ivec4 offsets = sampleOffsets[neighborhood];

    ivec4 cd = terrainFetch(inout_cellCoord);
    ivec4 cdl = terrainFetch(inout_cellCoord + ivec2(offsets.x, offsets.y));
    ivec4 cdr = terrainFetch(inout_cellCoord + ivec2(offsets.z, offsets.w));

    int h1 = bitfieldExtract(cd.r, 0, 8);
    int h2 = bitfieldExtract(cdl.r, 0, 8);
    int h3 = bitfieldExtract(cdr.r, 0, 8);
    float elevation =       interpolate_height(h1, h2, h3, in_barycentric, neighborhood);
    float visibility =      extract_interpolate(cd.r, cdl.r, cdr.r, in_barycentric, 8, 8); // Leave un-normalized
    float geology =         extract_interpolate(cd.r, cdl.r, cdr.r, in_barycentric, 16, 16); // TODO: figure out what to do with this

    float understory =      extract_normalize_interpolate_expand(cd.g, cdl.g, cdr.g, in_barycentric, inout_radius, 0, 8);
    float canopy =          extract_normalize_interpolate_expand(cd.g, cdl.g, cdr.g, in_barycentric, inout_radius, 8, 8);
    float tracks =          extract_normalize_interpolate_expand(cd.g, cdl.g, cdr.g, in_barycentric, inout_radius, 16, 8);

    float humidityPref =    float(bitfieldExtractU(cd.b, 0, 8)) / 255.0;
    float surfacewater =    float(bitfieldExtractU(cd.b, 8, 8)) / 255.0;

    float biotemp =         float(bitfieldExtractU(cd.a, 0, 8)) / 255.0;

    visibility = max(visibility, float(visibilityOverride));
    // Drop non-visible terrain to just below sea level
    elevation = visibility == 0.0 ? -1.0 : elevation;

    vec3 unscaledPosition = vec3(in_position, elevation);
    //vec4 localPosition = vec4(unscaledPosition * WorldInfo.gridToWorld, 1.0);
    //vec3 unscaledPosition = in_position + vec3(0.0, 0.0, (65535.0 / 256.0) * in_cellIndex);
    vec3 localPosition = unscaledPosition * vec3(1.0, 1.0, 0.1);
    vec4 worldPosition = m * (in_chunkPosition + vec4(localPosition, 0.0));
    //vec4 worldPosition = in_position;
    inout_pos = worldPosition.xyz; // NB: Output before applying camera transform

    gl_Position = pv * worldPosition;

    inout_data1 = vec4(visibility, geology, understory, canopy);
    inout_data2 = vec4(tracks, humidityPref, surfacewater, biotemp);
}
