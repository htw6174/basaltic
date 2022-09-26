#version 450

precision mediump float;

layout(push_constant) uniform matPV {
    mat4 projection;
    mat4 view;
} MatPV;

//layout(set = 0, binding = 1) uniform isampler2D terrainSampler;

struct terrainData {
    int packed1;
    int packed2;
};

layout(set = 0, binding = 0) uniform worldInfo {
    int timeOfDay;
    // season info, weather, etc.
} WorldInfo;

layout(set = 0, binding = 1) readonly buffer terrainBuffer {
    terrainData data[];
} TerrainBuffer;

layout(location = 0) in vec3 in_position;

layout(location = 0) out vec3 out_color;

float rand(float x) {
    return fract(sin(x) * 43758.5453123);
}

void main()
{
    int elevation;
    int palleteIndex;
    //ivec2 dims = textureSize(terrainSampler, 0);
    int cell = gl_VertexIndex / 7;
    //ivec2 pos = ivec2(cell % dims.x, cell / dims.x);
    //ivec4 cellData = texelFetch(terrainSampler, pos, 0);
    //int height = cellData.x;
    terrainData cellData = TerrainBuffer.data[cell];
    // extract a 16 bit int and uint from one 32 bit int
    elevation = bitfieldExtract(cellData.packed1, 0, 16);
    palleteIndex = bitfieldExtract(cellData.packed1, 16, 16); // TODO: ensure sign bit isn't carried over (convert first arg to uint?)
    vec3 finalPosition = in_position + vec3(0, 0, elevation * 0.2);
    // TODO: color lookup with cellData.y
    gl_Position = MatPV.projection * MatPV.view * vec4(finalPosition, 1.0) * vec4(1.0, -1.0, 1.0, 1.0); // flip y so it draws correctly for now; transform matricies should do this automatically later

    out_color = vec3(rand(cell + 0.0), rand(cell + 0.3), rand(cell + 0.6));
}
