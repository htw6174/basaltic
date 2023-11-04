#version 300 es
uniform mat4 pv;
uniform mat4 m;
layout(location=0) in vec4 position;
out vec4 color;
void main() {
    gl_Position = pv * position;
    color = vec4(1.0, 0.0, 0.0, 1.0);
}
