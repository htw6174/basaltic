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

#define WORLD_RADIUS 81.48733f
#define WORLD_CIRCUMFERENCE 512.0f
#define TAU 6.28319f

layout(push_constant) uniform mvp {
    mat4 pv;
    mat4 m;
} MVP;

struct terrainData {
    int packed1;
    int packed2;
};

layout(set = 0, binding = 0) uniform windowInfo {
	vec2 windowSize;
	vec2 mousePosition;
	vec3 cameraPosition;
	vec3 cameraFocalPoint;
} WindowInfo;

// also available in the fragment stage
layout(std140, set = 0, binding = 2) uniform worldInfo {
    int timeOfDay;
    // season info, weather, etc.
} WorldInfo;

layout(std430, set = 3, binding = 0) readonly buffer terrainBuffer { // requires version 430 or ARB_shader_storage_buffer_object
    uint chunkIndex;
    terrainData data[];
} TerrainBuffer;

layout(location = 0) in vec3 in_position;

layout(location = 0) out vec3 out_color;
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

void main()
{
    uint cell = gl_VertexIndex / 7;
    terrainData cellData = TerrainBuffer.data[cell];
    // extract a 16 bit int and uint from one 32 bit int
    int elevation = bitfieldExtract(cellData.packed1, 0, 16);
    // convert to uint to ignore sign bit in bitfieldExtract
    uint uPacked1 = cellData.packed1;
    //uint paletteIndex = bitfieldExtract(uPacked1, 16, 16);
    uint paletteX = bitfieldExtract(uPacked1, 16, 8);
    uint paletteY = bitfieldExtract(uPacked1, 24, 8);
    vec4 localPosition = vec4(in_position + vec3(0, 0, elevation * 0.2), 1.0);
    vec4 globalPosition = MVP.m * localPosition;
    out_pos = globalPosition.xyz;
    // warp position around a cylinder
    float focusRelativeX = globalPosition.x - WindowInfo.cameraFocalPoint.x;
    float theta = (focusRelativeX / WORLD_CIRCUMFERENCE) * TAU;
    vec4 warpedPosition = vec4(
        WindowInfo.cameraFocalPoint.x + (sin(theta) * WORLD_RADIUS) + (globalPosition.z * sin(theta)),
        globalPosition.y,
        (globalPosition.z * cos(theta)) + (cos(theta) * WORLD_RADIUS) - WORLD_RADIUS,
        1.0);
    gl_Position = MVP.pv * warpedPosition * vec4(1.0, -1.0, 1.0, 1.0); // flip y so it draws correctly for now; transform matricies should do this automatically later

    //out_color = vec3(rand(cell + 0.0), rand(cell + 0.3), rand(cell + 0.6));
    //out_color = cosGrad(paletteIndex / 255.0);
    out_color = vec3(paletteX / 255.0, paletteY / 255.0, 0.0);
    out_chunkIndex = TerrainBuffer.chunkIndex;
    out_cellIndex = cell;
}
