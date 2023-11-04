#version 300 es
precision mediump float;

in vec4 inout_color;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out float out_extra;

void main()
{
	out_color = inout_color;
	out_normal = vec3(0.5);
	out_extra = 0.0;
}
