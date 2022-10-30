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

layout(push_constant) uniform matPV {
    mat4 projection;
    mat4 view;
} MatPV;

struct terrainData {
    int packed1;
    int packed2;
};

layout(std140, set = 0, binding = 2) uniform worldInfo {
    int timeOfDay;
    // season info, weather, etc.
} WorldInfo;

layout(std430, set = 3, binding = 0) readonly buffer terrainBuffer { // requires version 430 or ARB_shader_storage_buffer_object
    terrainData data[];
} TerrainBuffer;

layout(location = 0) in vec3 in_position;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_pos;
layout(location = 2) out flat int out_cellIndex;

// float rand(float x) {
//     return fract(sin(x) * 43758.5453123);
// }
//
// vec3 cosGrad(float t) {
//     return terrainGrad.bias + terrainGrad.amp * cos(6.28318 * (terrainGrad.freq * t + terrainGrad.phase));
// }

void main()
{
    int cell = gl_VertexIndex / 7;
    terrainData cellData = TerrainBuffer.data[cell];
    // extract a 16 bit int and uint from one 32 bit int
    int elevation = bitfieldExtract(cellData.packed1, 0, 16);
    // convert to uint to ignore sign bit in bitfieldExtract
    uint uPacked1 = cellData.packed1;
    //uint paletteIndex = bitfieldExtract(uPacked1, 16, 16);
    uint paletteX = bitfieldExtract(uPacked1, 16, 8);
    uint paletteY = bitfieldExtract(uPacked1, 24, 8);
    vec3 finalPosition = in_position + vec3(0, 0, elevation * 0.2);
    out_pos = finalPosition;
    gl_Position = MatPV.projection * MatPV.view * vec4(finalPosition, 1.0) * vec4(1.0, -1.0, 1.0, 1.0); // flip y so it draws correctly for now; transform matricies should do this automatically later

    //out_color = vec3(rand(cell + 0.0), rand(cell + 0.3), rand(cell + 0.6));
    //out_color = cosGrad(paletteIndex / 255.0);
    out_color = vec3(paletteX / 255.0, paletteY / 255.0, 0.0);
    out_cellIndex = cell;
}
