#version 330

uniform mat4 inverse_view;
uniform mat4 inverse_projection;

layout(location = 0) in vec2 pos;
out vec2 uv;
out vec4 view_dir;
out vec4 world_dir;

void main() {
    gl_Position = vec4(pos*2.0 - 1.0, 0.5, 1.0);
    uv = pos;

    // To reconstruct view space position from the depth buffer
    // From David Lenaerts: https://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer/
    view_dir = inverse_projection * vec4(gl_Position.xy, 0.0, 1.0);
    view_dir /= view_dir.w;
    view_dir /= view_dir.z;
    world_dir = inverse_view * vec4(view_dir.xyz, 1.0);
}


