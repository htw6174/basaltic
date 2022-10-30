#version 420

layout(set = 2, binding = 0) uniform displayInfo { // TODO: write to and use this uniform. NOTE: May be better to normalize glyph rect sizes and just pass the aspect ratio here
    //int width;
    //int height;
    //float aspect;
    mat4 model;
} DisplayInfo;

// layout(set = 0, binding = 1) uniform stringTransforms { // TODO: what's better here: a dynamic uniform buffer with one draw call per string, or a storage buffer that can be indexed into? For now will just pass a single model matrix through existing buffer
//     mat4 model;
// } StringTransforms;

layout(location = 0) in vec4 in_position;

layout(location = 0) out vec2 out_texCoord;

void main() {
    vec2 ndc = in_position.xy / vec2(1280.0, 720.0);
    vec4 pos = vec4(ndc, 0.0, 1.0);
    gl_Position = DisplayInfo.model * pos;
    //gl_Position = pos;
    out_texCoord = in_position.zw;
}
