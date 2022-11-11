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

#define WORLD_RADIUS 81.48733f
#define WORLD_CIRCUMFERENCE 512.0f
#define TAU 6.28319f

layout(push_constant) uniform mvp {
    mat4 pv;
    mat4 m;
} MVP;

// TODO: move terrain data buffer structs to a header file for other shaders?
struct terrainData {
    int packed1;
    uint packed2;
};

layout(std430, set = 3, binding = 0) readonly buffer terrainBuffer { // requires version 430 or ARB_shader_storage_buffer_object
    uint chunkIndex;
    terrainData data[];
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
    terrainData cellData = TerrainBuffer.data[cellIndex];
    // extract a 16 bit int and uint from one 32 bit int
    int elevation = bitfieldExtract(cellData.packed1, 0, 16);
    // convert to uint to ignore sign bit in bitfieldExtract
    uint uPacked1 = cellData.packed1;
    //uint paletteIndex = bitfieldExtract(uPacked1, 16, 16);
    uint paletteX = bitfieldExtract(uPacked1, 16, 8);
    uint paletteY = bitfieldExtract(uPacked1, 24, 8);
    uint geometryVisibility = bitfieldExtract(cellData.packed2, 0, 1); // TODO: figure out a visual effect to switch on this
    uint colorVisibility = bitfieldExtract(cellData.packed2, 1, 1);

    vec4 localPosition = vec4(in_position + vec3(0, 0, elevation * WorldInfo.gridToWorld.z), 1.0);
    vec4 worldPosition = MVP.m * localPosition;
    out_pos = worldPosition.xyz;
    // warp position for a false horizon
    //worldPosition = sphereWarp(worldPosition);
    gl_Position = MVP.pv * worldPosition * vec4(1.0, -1.0, 1.0, 1.0); // flip y so it draws correctly for now; transform matricies should do this automatically later

    //out_color = vec3(rand(cellIndex + 0.0), rand(cellIndex + 0.3), rand(cellIndex + 0.6));
    //out_color = cosGrad(paletteIndex / 255.0);
    vec3 cellColor;
    if (colorVisibility == 1) cellColor = vec3(paletteX / 255.0, paletteY / 255.0, 0.0);
    else cellColor = vec3(0.5, 0.5, 0.5);
    out_color = vec4(cellColor, 1.0 * geometryVisibility);
    out_chunkIndex = TerrainBuffer.chunkIndex;
    out_cellIndex = cellIndex;
}
