#version 430

//precision mediump float;

//#include "uniforms.h"

uniform mat4 pv;
uniform mat4 m;

uniform isampler2D terrain;

struct CellData {
    int elevation;
    int temperature;
    int nutrient;
    int rainfall;
    uint visibility;
    int vegetation;
};

layout(std430, binding = 1) readonly buffer terrainBuffer { // requires version 430 or ARB_shader_storage_buffer_object
    CellData data[];
} TerrainBuffer;

// per-instance
layout(location = 0) in vec4 in_chunkPosition; // NOTE: this has a 1.0 w component built in. Remember not to add to other w components!
layout(location = 1) in vec2 in_rootCoord;
// per-vertex
layout(location = 2) in vec3 in_position;
layout(location = 3) in vec2 in_localCoord; // For compatability, this is a u16 normalized to a float. Multiply by 2e16 to restore.

out vec4 inout_color;
out vec3 inout_pos;
flat out uint inout_cellIndex;

float rand(float x) {
    return fract(sin(x) * 43758.5453123);
}

void main()
{
    inout_cellIndex = 0; //uint(65535.0 * in_cellIndex);

    // unpack terrain data
    // TEST: texel samples
    ivec2 cellCoord = ivec2(in_rootCoord) + ivec2(in_localCoord * 65535.0);

    // TODO: texelFetch is undefined on out of bounds access. Switch to a normalized texture function instead
    ivec4 cd = texelFetch(terrain, cellCoord, 0);

    int elevation = cd.r;

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
