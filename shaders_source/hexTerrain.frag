#version 450

precision mediump float;

#include "uniforms.h"

#define SPECULAR 0.5
#define DIFFUSE 0.7
#define REFLECTION 1.0
#define SHININESS 10.0

#define LIGHT_AMBIENT 0.3
#define LIGHT1_SPECULAR 1.0
#define LIGHT1_DIFFUSE 1.0

struct cosGradParams {
    vec3 bias;
    vec3 amp;
    vec3 freq;
    vec3 phase;
};

cosGradParams daylightGrad = cosGradParams(
    vec3(0.5, 0.5, 0.6),
    vec3(0.5, 0.5, 0.4),
    vec3(1.0, 1.0, 1.0),
    vec3(0.4, 0.5, 0.6)
);

const vec3 cliffColor = vec3(0.3, 0.2, 0.1);

// NOTE: because we only want to write to the feedback buffer from visible fragments, early depth testing is required
layout(early_fragment_tests) in;

layout(std430, set = 0, binding = 1) buffer feedbackBuffer {
	uint hoveredChunk;
	uint hoveredCell;
} FeedbackBuffer;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec3 in_pos;
layout(location = 2) in flat uint in_chunkIndex;
layout(location = 3) in flat uint in_cellIndex;

layout(location = 0) out vec4 out_color;

// float rand(float x) {
//     return fract(sin(x) * 43758.5453123);
// }
//
// vec3 randColor(float seed) {
// 	return vec3(rand(seed), rand(seed + 13.0), rand(seed + 37.0));
// }

vec3 cosGrad(float t, cosGradParams params) {
    return params.bias + params.amp * cos(6.28318 * (params.freq * t + params.phase));
}

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

float visibleArea(in float radius, in float edgeWidth) {
	float worldDistance = distance(in_pos, WindowInfo.cameraFocalPoint.xyz);
	float falloff = smoothstep(radius - edgeWidth, radius, worldDistance);
	return mix(1.0, 0.0, falloff);
}

// Adapted from https://iquilezles.org/articles/fog/
vec3 fog(in vec3 color, in float viewDistance) {
	float density = 1.0 - exp(-viewDistance * WindowInfo.fogExtinction);
	vec3 fogColor = vec3(0.5,0.6,0.7);
	return mix(color, fogColor, density);
}

vec3 analyticFog(in vec3 color, in float cameraDistance, in vec3 cameraPosition, in vec3 surfacePosition) {
	vec3 rayDir = surfacePosition - cameraPosition;
	float a = WindowInfo.fogInscattering;
	float b = WindowInfo.fogExtinction;
	// TODO: cap contribution from height difference?
	float fogAmount = (a / b) * exp(-cameraPosition.z * b) * (1.0 - exp(-cameraDistance * rayDir.z * b )) / rayDir.z;
	fogAmount = clamp(abs(fogAmount), 0.0, 1.0);
	vec3 fogColor = vec3(0.5,0.6,0.7);
	return mix(color, fogColor, fogAmount);
}

void main()
{
	// compute normal from position deriviatives
	vec3 normal = normalize(cross(dFdy(in_pos), dFdx(in_pos)));

	float focusDistance = distance(in_pos, WindowInfo.cameraFocalPoint.xyz);
	float cameraDistance = distance(in_pos, WindowInfo.cameraPosition.xyz);
	float viewDistance = min(
		focusDistance,
		cameraDistance
	);

	vec2 windowPos = gl_FragCoord.xy;
	vec2 mousePos = WindowInfo.mousePosition;
	float mouseDist = distance(windowPos, mousePos);
	if (mouseDist < 1.0) {
		FeedbackBuffer.hoveredChunk = in_chunkIndex;
		FeedbackBuffer.hoveredCell = in_cellIndex;
	}
	float cliff = 1.0 - normal.z;
	vec3 surfaceColor = mix(in_color.rgb, cliffColor, cliff * cliff);

	float dayLight = cos(WorldInfo.time * TAU / 24.0);
	vec3 litColor = surfaceColor * phong(normal, normalize(vec3(1.5, -3.0, 3.0)));
	//litColor *= cosGrad(WorldInfo.time / 24.0, daylightGrad);
	litColor = gl_FrontFacing ? litColor : vec3(0.0, 0.0, 0.0);
	//litColor = fog(litColor, viewDistance);
	vec3 fogViewOrigin = vec3(WindowInfo.cameraPosition.xy, 20.0); //min(20.0, WindowInfo.cameraPosition.z));
	litColor = analyticFog(litColor, viewDistance, fogViewOrigin, in_pos);

	//vec2 mouseNormalized = mousePos / WindowInfo.windowSize;
	//litColor = vec3(mouseNormalized, 1.0 - (mouseDist / 10.0));
	//litColor = WindowInfo.cameraFocalPoint / 64.0;

	out_color = vec4(litColor, visibleArea(WindowInfo.visibilityRadius, 16.0)); // TODO: consider another use for geometry visibility if chunks drawn later aren't blended properly
	//out_color = vec4(in_color, 1.0);
	//out_color = vec4(normal, 1.0);
}
