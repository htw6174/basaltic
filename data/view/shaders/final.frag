#version 300 es
precision mediump float;

uniform sampler2D image;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(image, uv).rgb, 1.0);
}

