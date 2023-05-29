#version 430
/*
 * Normal resolve adapted from Mikkelson[2020] - https://github.com/mmikk/surfgrad-bump-standalone-demo
 * Hex tile blending technique adapted from Mikkelson[2022] - https://github.com/mmikk/hextile-demo/
 *
 */

//precision mediump float;

#define PI 3.14159

// Smallest such that 1.0 + FLT_EPSILON != 1.0.
#ifndef FLT_EPSILON
#define FLT_EPSILON 1.192092896e-07F
#endif

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

// TODO: need to setup different terrain pipeline where these aren't needed
uniform sampler2D tex_color;
uniform sampler2D tex_normal;

layout(std430, binding = 0) buffer feedbackBuffer {
	int hoveredX;
	int hoveredY;
} FeedbackBuffer;

in vec4 inout_color;
in vec3 inout_pos;
in vec3 inout_normal;
flat in ivec2 inout_cellCoord;

out vec4 out_color;

// BookOfShaders rand and 2d rand
float rand(float x) {
	return fract(sin(x) * 43758.5453123);
}

float rand2(vec2 x) {
	return fract(sin(dot(x.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

vec2 hash2(vec2 p) {
	vec2 r = mat2x2(127.1, 311.7, 269.5, 183.3) * p;
	return fract( sin( r )*43758.5453 );
}

vec3 randColor(float seed) {
	return vec3(rand(seed), rand(seed + 13.0), rand(seed + 37.0));
}

// NOTE: the glsl built-in noise functions do nothing. naming convention for my functions: noise[input dimensions][output dimensions][type]
float noise21sharp(vec2 x) {
	vec2 i = floor(x);  // integer
	vec2 f = fract(x);  // fraction

	float a = rand2(i);
	float b = rand2(i + vec2(1.0, 0.0));
	float c = rand2(i + vec2(0.0, 1.0));
	float d = rand2(i + vec2(1.0, 1.0));

	return mix(a, b, f.x) +
	(c - a)* f.y * (1.0 - f.x) +
	(d - b) * f.x * f.y;
}

// Based on IQ's fBM article - https://iquilezles.org/articles/fbm/
float fbm(vec2 x, float H) {
	float G = exp2(-H);
	float f = 1.0;
	float a = 1.0;
	float t = 0.0;
	for (int i = 0; i < 2; i++) {
		t += a * noise21sharp(f * x); // Can use any continuous noise function which returns a scalar
		f *= 2.0;
		a *= G;
	}
	return t;
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

void main()
{
	// Determine if mouse is over this cell, write to host-readable buffer if so
	// NOTE: gl pixel centers lie on half integers. mousePosition should have 0.5 added to match OpenGL's pixel space.
	vec2 windowPos = gl_FragCoord.xy;
	float mouseDist = distance(windowPos, mousePosition);
	if (mouseDist < 0.1) {
		FeedbackBuffer.hoveredX = inout_cellCoord.x;
		FeedbackBuffer.hoveredY = inout_cellCoord.y;
	}

	// compute normal from position deriviatives
	// NOTE: computing normals in the pixel shader alone restricts you to flat shading; smooth shading requires information about neighboring verticies. If adding neighboring tile info anyway, could compute per vertex normals in vert shader?
	vec3 flatNormal = normalize(cross(dFdx(inout_pos), dFdy(inout_pos)));
	//float cliff = 1.0 - normal.z;
	bool isCliff = /*(flatNormal.z < sin(PI / 4)) &&*/ (inout_normal.z < sin(PI / 3.75));

	vec3 vN = isCliff ? flatNormal : inout_normal;

	//float normMix = smoothstep();
	//vec3 vN = mix(flatNormal, inout_normal, inout_normal.z);

	// Sample color
	// TODO: pick from colormap
	float elevation = inout_color.r;
	float biotemp = inout_color.g;
	float understory = inout_color.b;
	float humidityPref = inout_color.a;
	vec3 coldColor = vec3(0.9);
	vec3 warmColor = vec3(mix(0.0, 0.5, biotemp), understory, humidityPref);
	float snowLine = step(0.15, biotemp);
	vec3 biomeColor = mix(coldColor, warmColor, snowLine);

	float noiseFreq = isCliff ? 20.0 : 4.0;
	float noiseMag = isCliff ? 0.1 : 0.5;
	float bandFreq = isCliff ? 20.0 : 2.0;

	//float noisyZ = inout_pos.z + (sin(inout_pos.x * 2.0) * 0.2);
	float noisyZ = inout_pos.z + (noise21sharp(inout_pos.xy * noiseFreq) * noiseMag);
	//float noisyZ = inout_pos.z + (fbm(inout_pos.xy * 20.0, 1.0) * 0.1);
	float strataSample = floor(noisyZ * bandFreq);
	float cliffValue = (rand(strataSample) * 0.2) + 0.4;
	vec3 albedo = isCliff ? vec3(cliffValue) : biomeColor * (1.0 + (cliffValue - 0.5));

	// TODO: proper water level recoloring
	if (elevation < 0.0) albedo = vec3(0.5, 0.3, 0.1) / -(elevation - 1.0);

	// TEST: color per tile with obvious faces
	//albedo = randColor(rand2(inout_cellCoord));
	//vN = flatNormal;

	// Apply lighting
	vec3 litColor = albedo * phong(vN, normalize(vec3(1.5, -3.0, 3.0)));

	// display white outline on hovered cell
 	if (FeedbackBuffer.hoveredX == inout_cellCoord.x && FeedbackBuffer.hoveredY == inout_cellCoord.y) {
// 		float edgeDist = max(inout_uv.x, inout_uv.y) - (1.0 - (inout_uv.x + inout_uv.y));
// 		litColor = abs(edgeDist) < 0.1 ? vec3(1.0, 1.0, 1.0) : litColor;
		// For lack of UV coords, can add a slight highlight instead
		litColor *= 1.3;
	}

	//litColor = mix(litColor, vec3(1.0, 0.0, 0.0), 1.0 - (mouseDist));

	//out_color = vec4(litColor, 1.0);
	//out_color = inout_color;
	out_color = vec4(litColor, 1.0);
	//out_color = vec4(inout_uv, 0.0, 1.0);
}
