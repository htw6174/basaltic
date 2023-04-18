#version 430

//precision mediump float;

#define SPECULAR 0.5
#define DIFFUSE 0.7
#define REFLECTION 1.0
#define SHININESS 10.0

#define LIGHT_AMBIENT 0.3
#define LIGHT1_SPECULAR 1.0
#define LIGHT1_DIFFUSE 1.0

//#include "uniforms.h"

//const vec3 cliffColor = vec3(0.3, 0.2, 0.1);

// NOTE: because we only want to write to the feedback buffer from visible fragments, early depth testing is required
layout(early_fragment_tests) in;

uniform vec2 mousePosition;

layout(std430, binding = 0) buffer feedbackBuffer {
	uint hoveredChunk;
	uint hoveredCell;
} FeedbackBuffer;

//buffer uint hoveredCell;

in vec4 inout_color;
in vec3 inout_pos;
flat in uint inout_chunkIndex;
flat in uint inout_cellIndex;

out vec4 out_color;

float phong(vec3 normal, vec3 lightDir) {
	float ambient = REFLECTION * LIGHT_AMBIENT;
	vec3 reflection = normalize(reflect(-lightDir, normal));
	float dotLN = dot(lightDir, normal);
	if (dotLN < 0.0) {
		return ambient;
	}
	float diffuse = DIFFUSE * dotLN * LIGHT1_DIFFUSE;
	return ambient + diffuse;
}

float rand(float x) {
    return fract(sin(x) * 43758.5453123);
}

vec3 randColor(float seed) {
	return vec3(rand(seed), rand(seed + 13.0), rand(seed + 37.0));
}

void main()
{
	// compute normal from position deriviatives
	vec3 normal = normalize(cross(dFdx(inout_pos), dFdy(inout_pos)));

	vec2 windowPos = gl_FragCoord.xy;
	vec2 mousePos = mousePosition * vec2(1.0, -1.0) + vec2(0.0, 720.0);
	float mouseDist = distance(windowPos, mousePos);
	if (mouseDist < 1.0) {
		FeedbackBuffer.hoveredChunk = inout_chunkIndex;
		FeedbackBuffer.hoveredCell = inout_cellIndex;
	}

	//float cliff = 1.0 - normal.z;
	//vec3 surfaceColor = mix(in_color.rgb, cliffColor, cliff * cliff);
	//vec3 randColor = randColor(inout_cellIndex);

	vec3 litColor = inout_color.rgb * phong(normal, normalize(vec3(1.5, -3.0, 3.0)));

	//vec2 mouseNormalized = mousePos / WindowInfo.windowSize;
	//litColor = vec3(mouseNormalized, 1.0 - (mouseDist / 10.0));

	out_color = vec4(litColor, 1.0);
	//out_color = inout_color;
	//out_color = vec4(normal, 1.0);
}
