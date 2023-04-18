#version 430

//precision mediump float;

//#include "uniforms.h"

uniform mat4 pv;
uniform mat4 m;

// TODO: move terrain data buffer structs to a header file for other shaders?
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

layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_cellIndex; // For compatability, this is a u16 normalized to a float. Multiply by 2e16 to restore.

out vec4 inout_color;
out vec3 inout_pos;
flat out uint inout_cellIndex;

// float rand(float x) {
//     return fract(sin(x) * 43758.5453123);
// }

void main()
{
    inout_cellIndex = uint(65535.0 * in_cellIndex);

    // unpack terrain data
    CellData cellData = TerrainBuffer.data[inout_cellIndex];
    int elevation = cellData.elevation;
    //uint visibilityBits = bitfieldExtract(cellData.visibility, 0, 8);
    //visibilityBits = visibilityBits | WorldInfo.visibilityOverrideBits;

    //float waterDepth = (WorldInfo.seaLevel - elevation) / 64.0; // >0 on ocean

    //elevation = max(elevation, WorldInfo.seaLevel);

    vec3 unscaledPosition = in_position + vec3(0.0, 0.0, elevation);
    //vec4 localPosition = vec4(unscaledPosition * WorldInfo.gridToWorld, 1.0);
    //vec3 unscaledPosition = in_position + vec3(0.0, 0.0, (65535.0 / 256.0) * in_cellIndex);
    vec4 localPosition = vec4(unscaledPosition * vec3(1.0, 1.0, 0.2), 1.0);
    vec4 worldPosition = m * localPosition;
    //vec4 worldPosition = in_position;
    inout_pos = worldPosition.xyz; // NB: Output before applying camera transform

    gl_Position = pv * worldPosition;

    //out_color = vec3(rand(cellIndex + 0.0), rand(cellIndex + 0.3), rand(cellIndex + 0.6));

    //inout_color = vec4(1.0, 0.0, 0.0, 1.0);
    inout_color = vec4(cellData.temperature / 255.0, cellData.vegetation / 255.0, cellData.rainfall / 255.0, 1.0);
}
