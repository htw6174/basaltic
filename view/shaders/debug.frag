#version 330

precision mediump float;

in vec4 inout_color;

out vec4 out_color;

void main()
{
	out_color = inout_color;
}
