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
uniform int chunkIndex;

layout(std430, binding = 0) buffer feedbackBuffer {
	int hoveredX;
	int hoveredY;
} FeedbackBuffer;

in vec4 inout_color;
in vec3 inout_pos;
in vec3 inout_normal;
in vec2 inout_uv;
flat in ivec2 inout_cellCoord;

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
	// NOTE: computing normals in the pixel shader alone restricts you to flat shading; smooth shading requires information about neighboring verticies. If adding neighboring tile info anyway, could compute per vertex normals in vert shader?
	//vec3 normal = normalize(cross(dFdx(inout_pos), dFdy(inout_pos)));
	vec3 normal = inout_normal;

	// NOTE: gl pixel centers lie on half integers. mousePosition should have 0.5 added to match OpenGL's pixel space.
	vec2 windowPos = gl_FragCoord.xy;
	float mouseDist = distance(windowPos, mousePosition);
	if (mouseDist < 0.1) {
		FeedbackBuffer.hoveredX = inout_cellCoord.x;
		FeedbackBuffer.hoveredY = inout_cellCoord.y;
	}

	//float cliff = 1.0 - normal.z;
	//vec3 surfaceColor = mix(in_color.rgb, cliffColor, cliff * cliff);
	//vec3 randColor = randColor(inout_cellIndex);

	vec3 litColor = inout_color.rgb * phong(normal, normalize(vec3(1.5, -3.0, 3.0)));

	// display white outline on hovered cell
	if (FeedbackBuffer.hoveredX == inout_cellCoord.x && FeedbackBuffer.hoveredY == inout_cellCoord.y) {
		float edgeDist = max(inout_uv.x, inout_uv.y) - (1.0 - (inout_uv.x + inout_uv.y));
		litColor = abs(edgeDist) < 0.1 ? vec3(1.0, 1.0, 1.0) : litColor;
	}

	//litColor = mix(litColor, vec3(1.0, 0.0, 0.0), 1.0 - (mouseDist));

	out_color = vec4(litColor, 1.0);
	//out_color = inout_color;
	//out_color = vec4(normal, 1.0);
	//out_color = vec4(inout_uv, 0.0, 1.0);
	//out_color = vec4(rand(inout_cellCoord.x), rand(inout_cellCoord.y), rand(dot(inout_cellCoord, inout_cellCoord)), 1.0);
}
