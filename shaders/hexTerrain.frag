#version 450

precision mediump float;

// NOTE: because we only want to write to the view buffer from visible fragments, early depth testing is required
layout(early_fragment_tests) in;

layout(set = 1, binding = 0) uniform windowInfo {
	vec2 windowSize;
	vec2 mousePosition;
} WindowInfo;

layout(std430, set = 1, binding = 1) buffer viewBuffer {
	int hoveredCell;
} ViewBuffer;

layout(location = 0) in vec3 in_color;
layout(location = 1) in flat int in_cellIndex;

layout(location = 0) out vec4 out_color;

// float rand(float x) {
//     return fract(sin(x) * 43758.5453123);
// }
//
// vec3 randColor(float seed) {
// 	return vec3(rand(seed), rand(seed + 13.0), rand(seed + 37.0));
// }

void main()
{
	vec2 windowPos = gl_FragCoord.xy;
	vec2 mousePos = WindowInfo.mousePosition;
	float mouseDist = distance(windowPos, mousePos);
	if (mouseDist < 1.0) {
		ViewBuffer.hoveredCell = in_cellIndex;
	}
	out_color = vec4(in_color, 1.0);
}
