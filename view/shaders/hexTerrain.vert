#version 430

//precision mediump float;

//#include "uniforms.h"

ivec4 sampleOffsets[7] = ivec4[](
    // for verticies that shouldn't be affected by neighboring tiles, helps avoid branching
    ivec4(0, 0, 0, 0),
    // Begins from samples used for north vertex, in clockwise order
    ivec4(-1, 1, 0, 1),
    ivec4(0, 1, 1, 0),
    ivec4(1, 0, 1, -1),
    ivec4(1, -1, 0, -1),
    ivec4(0, -1, -1, 0),
    ivec4(-1, 0, -1, 1)
);

uniform mat4 pv;
uniform mat4 m;

uniform isampler2D terrain;

// per-instance
layout(location = 0) in vec4 in_chunkPosition; // NOTE: this has a 1.0 w component built in. Remember not to add to other w components!
layout(location = 1) in vec2 in_rootCoord;
// per-vertex
layout(location = 2) in vec3 in_position;
layout(location = 3) in float in_neighborWeight; // integral part is index into sampleOffsets, fractional part is weight from left to right sample
layout(location = 4) in vec2 in_localCoord; // For compatability, this is a u16 normalized to a float. Multiply by 2e16 to restore.

out vec4 inout_color;
out vec3 inout_pos;
flat out ivec2 inout_cellCoord;
//flat out uint inout_cellIndex;

float rand(float x) {
    return fract(sin(x) * 43758.5453123);
}

ivec4 terrainFetch(ivec2 coord) {
    // texelFetch is undefined on out of bounds access. Must manually wrap first
    ivec2 wrap = textureSize(terrain, 0);
    return texelFetch(terrain, (coord + wrap) % wrap, 0);
}

void main()
{
    //inout_cellIndex = uint(65535.0 * in_cellIndex);

    // unpack terrain data
    inout_cellCoord = ivec2(in_rootCoord) + ivec2(in_localCoord * 65535.0);
    ivec4 cd = terrainFetch(inout_cellCoord);

    // get neighboring cell data
    ivec4 offsets = sampleOffsets[int(floor(in_neighborWeight))];
    ivec4 cdl = terrainFetch(inout_cellCoord + ivec2(offsets.x, offsets.y));
    ivec4 cdr = terrainFetch(inout_cellCoord + ivec2(offsets.z, offsets.w));

    // If either neighbor is close enough, make elevation average of self and neighbors
    int slopeLeft = cdl.r - cd.r;
    int slopeRight = cdr.r - cd.r;
    float tweakLeft = abs(slopeLeft) < 2 ? 0.5 : 0.0;
    float tweakRight = abs(slopeRight) < 2 ? 0.5 : 0.0;

    float h = cd.r;
    float hl = cdl.r;
    float hr = cdr.r;

    float avgLeft = mix(h, hl, tweakLeft);
    float avgRight = mix(h, hr, tweakRight);

    float elevation = mix(avgLeft, avgRight, 0.5);
    //float avg = (cd.r + cdl.r + cdr.r) / 3;
    //float elevation = mix(float(cd.r), avg, tweak);
    //int elevation = (cd.r + cdl.r + cdr.r) / 3;

    //uint visibilityBits = bitfieldExtract(cellData.visibility, 0, 8);
    //visibilityBits = visibilityBits | WorldInfo.visibilityOverrideBits;

    //float waterDepth = (WorldInfo.seaLevel - elevation) / 64.0; // >0 on ocean

    //elevation = max(elevation, WorldInfo.seaLevel);

    // TEST: wiggle elevation up or down by a half step
    //float wiggle = round( sin(rand(gl_VertexID))) * 0.5;
    //float wiggle = cd.r;

    vec3 unscaledPosition = in_position + vec3(0.0, 0.0, elevation);
    //vec4 localPosition = vec4(unscaledPosition * WorldInfo.gridToWorld, 1.0);
    //vec3 unscaledPosition = in_position + vec3(0.0, 0.0, (65535.0 / 256.0) * in_cellIndex);
    vec3 localPosition = unscaledPosition * vec3(1.0, 1.0, 0.2);
    vec4 worldPosition = m * (in_chunkPosition + vec4(localPosition, 0.0));
    //vec4 worldPosition = in_position;
    inout_pos = worldPosition.xyz; // NB: Output before applying camera transform

    gl_Position = pv * worldPosition;

    //out_color = vec3(rand(cellIndex + 0.0), rand(cellIndex + 0.3), rand(cellIndex + 0.6));

    //inout_color = vec4(1.0, 0.0, 0.0, 1.0);
    inout_color = vec4(cd.g / 255.0, cd.b / 255.0, cd.a / 255.0, 1.0);
    //inout_color = vec4(in_rootCoord.x / 192.0, in_rootCoord.y / 192.0, 0.0, 1.0);
}
