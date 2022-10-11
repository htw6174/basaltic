#version 450

precision mediump float;

struct cosGradParams {
    vec3 bias;
    vec3 amp;
    vec3 freq;
    vec3 phase;
};

cosGradParams terrainGrad = cosGradParams(
    vec3(0.5, 0.5, 0.5),
    vec3(-0.5, 0.5, 0.6),
    vec3(2.0, 0.45, 1.0),
    vec3(0.5, 0.6, 1.8)
);

layout(push_constant) uniform matPV {
    mat4 projection;
    mat4 view;
} MatPV;

//layout(set = 0, binding = 1) uniform isampler2D terrainSampler;

struct terrainData {
    int packed1;
    int packed2;
};

layout(std140, set = 0, binding = 0) uniform worldInfo {
    int timeOfDay;
    // season info, weather, etc.
} WorldInfo;

layout(std430, set = 0, binding = 1) readonly buffer terrainBuffer { // requires version 430 or ARB_shader_storage_buffer_object
    terrainData data[];
} TerrainBuffer;

layout(location = 0) in vec3 in_position;

layout(location = 0) out vec3 out_color;

float rand(float x) {
    return fract(sin(x) * 43758.5453123);
}

vec3 cosGrad(float t) {
    return terrainGrad.bias + terrainGrad.amp * cos(6.28318 * (terrainGrad.freq * t + terrainGrad.phase));
}

void main()
{
    int elevation;
    int paletteIndex;
    //ivec2 dims = textureSize(terrainSampler, 0);
    int cell = gl_VertexIndex / 7;
    //ivec2 pos = ivec2(cell % dims.x, cell / dims.x);
    //ivec4 cellData = texelFetch(terrainSampler, pos, 0);
    //int height = cellData.x;
    terrainData cellData = TerrainBuffer.data[cell];
    // extract a 16 bit int and uint from one 32 bit int
    elevation = bitfieldExtract(cellData.packed1, 0, 16);
    paletteIndex = bitfieldExtract(cellData.packed1, 16, 16); // TODO: ensure sign bit isn't carried over (convert first arg to uint?)
    vec3 finalPosition = in_position + vec3(0, 0, elevation * 0.2);
    // TODO: color lookup with cellData.y
    gl_Position = MatPV.projection * MatPV.view * vec4(finalPosition, 1.0) * vec4(1.0, -1.0, 1.0, 1.0); // flip y so it draws correctly for now; transform matricies should do this automatically later

    //out_color = vec3(rand(cell + 0.0), rand(cell + 0.3), rand(cell + 0.6));
    out_color = cosGrad(paletteIndex / 255.0);
}
