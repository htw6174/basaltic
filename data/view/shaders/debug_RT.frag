#version 300 es
precision mediump float;

uniform sampler2D renderTarget;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(renderTarget, uv).xxx, 1.0);
}
