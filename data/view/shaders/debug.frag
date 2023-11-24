#version 300 es
precision mediump float;

in vec4 inout_color;

layout(location = 0) out vec4 out_color;
//layout(location = 1) out vec3 out_normal;
//layout(location = 2) out ivec2 out_id;

void main()
{
	out_color = vec4(inout_color.rgb, 0.5);
//	out_normal = vec3(0.5);
//	out_id = ivec2(0, 0);
}
