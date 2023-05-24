#version 430

//precision mediump float;

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

out vec4 inout_color;
out vec3 inout_pos;
out vec3 inout_normal;
out vec3 inout_tangent;
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

ivec4 terrainFetch(ivec2 coord) {
    // texelFetch is undefined on out of bounds access. Must manually wrap first
    ivec2 wrap = textureSize(terrain, 0);
    return texelFetch(terrain, (coord + wrap) % wrap, 0);
}

float height(vec3 barycentric, ivec2 cellCoord, int neighborhood) {
    ivec4 cd = terrainFetch(cellCoord);
    ivec4 offsets = sampleOffsets[neighborhood];
    ivec4 cdl = terrainFetch(cellCoord + ivec2(offsets.x, offsets.y));
    ivec4 cdr = terrainFetch(cellCoord + ivec2(offsets.z, offsets.w));

    // create sharp cliffs
    // warp barycentric space along one axis if slope in that direction is high enough
    int slopeLeft = cdl.r - cd.r;
    int slopeRight = cdr.r - cd.r;
    float remapLeft = abs(slopeLeft) > 2 ? 1.0 : 0.0;
    float remapRight = abs(slopeRight) > 2 ? 1.0 : 0.0;

    float slopeFactor = 0.15;

    // approximate local derivative
    // create 2 more barycentric coords by moving in both directions away from home cell
    const float planck = 0.001;
    // Right-hand rule for cross product results - index finger, towards cdr
    vec3 bary1 = vec3(barycentric.x - planck, barycentric.y, barycentric.z + planck);
    // middle finger, towards cdl
    vec3 bary2 = vec3(barycentric.x - planck, barycentric.y + planck, barycentric.z);

    // must remap after adding planck for accurate slope
    barycentric = remapBarycentric(barycentric, remapLeft, remapRight, slopeFactor);
    bary1 = remapBarycentric(bary1, remapLeft, remapRight, slopeFactor);
    bary2 = remapBarycentric(bary2, remapLeft, remapRight, slopeFactor);

    // Use barycentric coord to interpolate samples
    float height = (cd.r * barycentric.x) + (cdl.r * barycentric.y) + (cdr.r * barycentric.z);
    float h1 = (cd.r * bary1.x) + (cdl.r * bary1.y) + (cdr.r * bary1.z);
    float h2 = (cd.r * bary2.x) + (cdl.r * bary2.y) + (cdr.r * bary2.z);

    // get sample positions as cartesian coordinates
    // in the special case of an equilateral triangle, this is easy to find. cartesian distance == (barycentric distance * grid scale)
    // still need to rotate vectors according to sample cell's direction; == -(neighborhood - 2) * [60 deg, 30 deg]
    // More general formula : https://www.iue.tuwien.ac.at/phd/nentchev/node26.html#eq:lambda_representation_2D_xy
    float rot1 = (3.14159 / 3.0) * -(neighborhood - 1);
    float rot2 = (3.14159 / 3.0) * -(neighborhood - 2);
    // Get distance from vertex
    vec3 tangent = vec3(cos(rot1) * planck, sin(rot1) * planck, (h1 - height) * 0.2); // TODO: provide terrain scale as uniform, multiply in
    vec3 bitangent = vec3(cos(rot2) * planck, sin(rot2) * planck, (h2 - height) * 0.2);

    // approximate vertex normal from cross product - thumb
    inout_normal = normalize(cross(tangent, bitangent));
    //inout_normal = barycentric;

    // provide tangent to frag shader for mikkTSpace stuff
    inout_tangent = normalize(vec3(planck, 0.0, (h1 - height) * 0.2));

    // TEST: see what this looks like
    //inout_color = vec4(barycentric, 1.0);

    return height;
}

void main()
{
    // unpack terrain data
    inout_cellCoord = ivec2(in_rootCoord) + ivec2(in_localCoord * 32767.0); // scale normalized s16 back to full range
    ivec4 cd = terrainFetch(inout_cellCoord);

    // get neighboring cell data
    int neighborhood = int(floor(in_neighborWeight));

    float elevation = height(in_barycentric, inout_cellCoord, neighborhood);
    //float elevation = float(cd.r);

    //uint visibilityBits = bitfieldExtract(cellData.visibility, 0, 8);
    //visibilityBits = visibilityBits | WorldInfo.visibilityOverrideBits;

    //float waterDepth = (WorldInfo.seaLevel - elevation) / 64.0; // >0 on ocean

    //elevation = max(elevation, WorldInfo.seaLevel);

    // TEST: wiggle elevation up or down by a half step
    //float wiggle = round( sin(rand(gl_VertexID))) * 0.5;
    //float wiggle = cd.r;

    vec3 unscaledPosition = vec3(in_position, elevation);
    //vec4 localPosition = vec4(unscaledPosition * WorldInfo.gridToWorld, 1.0);
    //vec3 unscaledPosition = in_position + vec3(0.0, 0.0, (65535.0 / 256.0) * in_cellIndex);
    vec3 localPosition = unscaledPosition * vec3(1.0, 1.0, 0.2);
    vec4 worldPosition = m * (in_chunkPosition + vec4(localPosition, 0.0));
    //vec4 worldPosition = in_position;
    inout_pos = worldPosition.xyz; // NB: Output before applying camera transform

    gl_Position = pv * worldPosition;

    //out_color = vec3(rand(cellIndex + 0.0), rand(cellIndex + 0.3), rand(cellIndex + 0.6));

    //inout_color = vec4(in_barycentric, 1.0);
    inout_color = vec4(cd.g / 255.0, cd.b / 255.0, cd.a / 255.0, 1.0);
}

