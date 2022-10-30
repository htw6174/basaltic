#version 420

layout(set = 2, binding = 1) uniform sampler2D face;

layout(location = 0) in vec2 in_texCoord;

layout(location = 0) out vec4 out_color;

void main() {
    //out_color = vec4(1.0, 1.0, 1.0, 0.5);
    out_color = vec4(1.0, 1.0, 1.0, texture(face, in_texCoord).r);
    //vec3 col = mix(vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0), texture(face, in_texCoord).r);
    //out_color = vec4(col, 1.0);
}
