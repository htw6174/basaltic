#version 450

precision mediump float;

// struct cosGradParams {
//     vec3 bias;
//     vec3 amp;
//     vec3 freq;
//     vec3 phase;
// };
//
// cosGradParams terrainGrad = cosGradParams(
//     vec3(0.5, 0.5, 0.5),
//     vec3(-0.5, 0.5, 0.6),
//     vec3(2.0, 0.45, 1.0),
//     vec3(0.5, 0.6, 1.8)
// );

#include "uniforms.h"

layout(push_constant) uniform mvp {
    mat4 pv;
    mat4 m;
} MVP;

// TODO: move terrain data buffer structs to a header file for other shaders?
struct CellData {
    int elevation;
    int temperature;
    int nutrient;
    int rainfall;
    uint visibility;
    int vegetation;
};

layout(std430, set = 3, binding = 0) readonly buffer terrainBuffer { // requires version 430 or ARB_shader_storage_buffer_object
    uint chunkIndex;
    CellData data[];
} TerrainBuffer;

layout(location = 0) in vec3 in_position;
layout(location = 1) in uint cellIndex;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec3 out_pos;
layout(location = 2) out flat uint out_chunkIndex;
layout(location = 3) out flat uint out_cellIndex;

// float rand(float x) {
//     return fract(sin(x) * 43758.5453123);
// }
//
// vec3 cosGrad(float t) {
//     return terrainGrad.bias + terrainGrad.amp * cos(6.28318 * (terrainGrad.freq * t + terrainGrad.phase));
// }

vec4 cylinderWarp(vec4 worldPosition) {
    float worldCircumference = WorldInfo.totalWidth;
    float worldRadius = worldCircumference / TAU;
    float focusRelativeX = worldPosition.x - WindowInfo.cameraFocalPoint.x;
    float theta = (focusRelativeX / worldCircumference) * TAU;
    vec4 warpedPosition = vec4(
        WindowInfo.cameraFocalPoint.x + (sin(theta) * worldRadius) + (worldPosition.z * sin(theta)),
        worldPosition.y,
        (worldPosition.z * cos(theta)) + (cos(theta) * worldRadius) - worldRadius,
        1.0);
    return warpedPosition;
}

vec4 sphereWarp(vec4 worldPosition) {
    float worldCircumference = WorldInfo.totalWidth;
    float worldRadius = worldCircumference / TAU;
    vec2 focusRelative = worldPosition.xy - WindowInfo.cameraFocalPoint.xy;
    vec2 theta = (focusRelative / worldCircumference) * TAU;
    float latitude = min(cos(theta.x), cos(theta.y)); // TODO: figure out the correct way to modify z around a sphere
    vec4 warpedPosition = vec4(
        WindowInfo.cameraFocalPoint.x + (sin(theta.x) * worldRadius) + (worldPosition.z * sin(theta.x)),
        WindowInfo.cameraFocalPoint.y + (sin(theta.y) * worldRadius) + (worldPosition.z * sin(theta.y)),
        (worldPosition.z * latitude) + (latitude * worldRadius) - worldRadius,
        1.0);
    return warpedPosition;
}

void main()
{
    // unpack terrain data
    CellData cellData = TerrainBuffer.data[cellIndex];
    int elevation = cellData.elevation;
    uint visibilityBits = bitfieldExtract(cellData.visibility, 0, 8);
    visibilityBits = visibilityBits | WorldInfo.visibilityOverrideBits;

    float waterDepth = (WorldInfo.seaLevel - elevation) / 64.0; // >0 on ocean

    elevation = max(elevation, WorldInfo.seaLevel);

    vec4 localPosition = vec4(in_position + vec3(0, 0, elevation * WorldInfo.gridToWorld.z), 1.0);
    vec4 worldPosition = MVP.m * localPosition;
    // warp position for a false horizon
    //worldPosition = cylinderWarp(worldPosition);
    //worldPosition = sphereWarp(worldPosition);
    // can put this before or after horizon warping for different lighting effects
    out_pos = worldPosition.xyz;

    gl_Position = MVP.pv * worldPosition * vec4(1.0, -1.0, 1.0, 1.0); // flip y so it draws correctly for now; transform matricies should do this automatically later

    //out_color = vec3(rand(cellIndex + 0.0), rand(cellIndex + 0.3), rand(cellIndex + 0.6));
    //out_color = cosGrad(paletteIndex / 255.0);
    vec3 paletteSample = vec3(cellData.temperature / 255.0, cellData.vegetation / 255.0, cellData.rainfall / 255.0); // TODO: sample from palette textures
    paletteSample = elevation == WorldInfo.seaLevel ? vec3(0.0, 0.0, 1.0 - waterDepth) : paletteSample;
    vec3 cellColor = bool(visibilityBits & visibilityBitColor) ? paletteSample : vec3(0.3, 0.3, 0.3);
    float a = bool(visibilityBits & visibilityBitGeometry) ? 1.0 : 0.0;

	//vec2 mouseNormalized = WindowInfo.mousePosition / WindowInfo.windowSize;
	//cellColor = vec3(mouseNormalized, 0.0);

    out_color = vec4(cellColor, a);
    out_chunkIndex = TerrainBuffer.chunkIndex;
    out_cellIndex = cellIndex;
}
