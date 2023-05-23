#version 430
/*
 * Normal resolve adapted from Mikkelson[2020] - https://github.com/mmikk/surfgrad-bump-standalone-demo
 * Hex tile blending technique adapted from Mikkelson[2022] - https://github.com/mmikk/hextile-demo/
 *
 */

//precision mediump float;

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

uniform sampler2D tex_color;
uniform sampler2D tex_normal;

layout(std430, binding = 0) buffer feedbackBuffer {
	int hoveredX;
	int hoveredY;
} FeedbackBuffer;

in vec4 inout_color;
in vec3 inout_pos;
in vec3 inout_normal;
in vec3 inout_tangent;
flat in ivec2 inout_cellCoord;

out vec4 out_color;

vec3 dPdx;
vec3 dPdy;
vec3 sigmaX;
vec3 sigmaY;
vec3 mikktsTangent;
vec3 mikktsBitangent;
vec3 nrmBaseNormal;
float flip_sign;


float rand(float x) {
	return fract(sin(x) * 43758.5453123);
}

vec2 hash2(vec2 p) {
	vec2 r = mat2x2(127.1, 311.7, 269.5, 183.3) * p;
	return fract( sin( r )*43758.5453 );
}

vec3 randColor(float seed) {
	return vec3(rand(seed), rand(seed + 13.0), rand(seed + 37.0));
}

void GenBasisTB(out vec3 vT, out vec3 vB, vec2 texST) {
	vec2 dSTdx = dFdx(texST);
	vec2 dSTdy = dFdx(texST);
	float det = dot(dSTdx, vec2(dSTdy.y, -dSTdy.x));
	float sign_det = det < 0.0 ? -1.0 : 1.0;

	// invC0 represents (dXds, dYds), but we don't divide
	// by the determinant. Instead, we scale by the sign.
	vec2 invC0 = sign_det * vec2(dSTdy.y, -dSTdx.y);
	vT = sigmaX * invC0.x + sigmaY * invC0.y;
	if (abs(det) > 0.0) vT = normalize(vT);
	vB = (sign_det*flip_sign)*cross(nrmBaseNormal, vT);
}

// vM must be in [-1..1]
vec2 TspaceNormalToDerivative(vec3 vM) {
	const float scale = 1.0 / 128.0;

	// Ensure vM delivers a positive third component using abs() and
	// constrain vM.z so the range of the derivative is [-128; 128].
	const vec3 vMa = abs(vM);
	const float z_ma = max(vMa.z, scale * max(vMa.x, vMa.y));

	// Set to match positive vertical texture coordinate axis.
	const bool gFlipVertDeriv = true;
	const float s = gFlipVertDeriv ? -1.0 : 1.0;
	return -vec2(vM.x, s*vM.y) / z_ma;
}

vec3 SurfgradFromTBN(vec2 deriv, vec3 vT, vec3 vB) {
	return deriv.x * vT + deriv.y * vB;
}

vec3 ResolveNormalFromSurfaceGradient(vec3 surfGrad) {
	return normalize(nrmBaseNormal - surfGrad);
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
	// compute normal from position deriviatives
	// NOTE: computing normals in the pixel shader alone restricts you to flat shading; smooth shading requires information about neighboring verticies. If adding neighboring tile info anyway, could compute per vertex normals in vert shader?
	//vec3 normal = normalize(cross(dFdx(inout_pos), dFdy(inout_pos)));

	// Setup mikkTSpace requirements
	float sign_w = 1.0; // TODO: ever need to be -1 in my use case?
	mikktsTangent = inout_tangent;
	mikktsBitangent = sign_w * cross(inout_normal, inout_tangent);

	float renormFactor = 1.0 / length(inout_normal);
	mikktsTangent *= renormFactor;
	mikktsBitangent *= renormFactor;
	nrmBaseNormal = renormFactor * inout_normal;

	dPdx = dFdx(inout_pos);
	dPdy = dFdx(inout_pos);
	sigmaX = dPdx - dot(dPdx, nrmBaseNormal) * nrmBaseNormal;
	sigmaY = dPdy - dot(dPdy, nrmBaseNormal) * nrmBaseNormal;
	flip_sign = dot(dPdy, cross(nrmBaseNormal, dPdx)) < 0 ? -1 : 1;

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

	// Tile across cells, and add a random texture offset per cell
	vec2 samplePos = (inout_pos.xy * 0.1) + hash2(vec2(inout_cellCoord));

	// FIXME: need proper mikkTSpace normal+tangent from vert stage. For now, use an approximation
	//GenBasisTB(mikktsTangent, mikktsBitangent, samplePos);

	// Sample color
	//vec3 albedo = inout_color.rgb;
	//vec3 albedo = texelFetch(tex_color, ivec2(inout_pos.xy), 0).rgb;
	vec3 albedo = texture(tex_color, samplePos).rgb;
	albedo = mix(inout_color.xyz, albedo, inout_normal.z);

	// Sample normal
	vec3 sampleNormal = texture(tex_normal, samplePos).rgb;
	vec2 deriv = TspaceNormalToDerivative((2.0 * sampleNormal) - 1.0);
	vec3 surfGrad = SurfgradFromTBN(deriv, mikktsTangent, mikktsBitangent);
	vec3 vN = ResolveNormalFromSurfaceGradient(surfGrad);

	// Apply lighting
	vec3 litColor = albedo * phong(vN, normalize(vec3(1.5, -3.0, 3.0)));

	// display white outline on hovered cell
// 	if (FeedbackBuffer.hoveredX == inout_cellCoord.x && FeedbackBuffer.hoveredY == inout_cellCoord.y) {
// 		float edgeDist = max(inout_uv.x, inout_uv.y) - (1.0 - (inout_uv.x + inout_uv.y));
// 		litColor = abs(edgeDist) < 0.1 ? vec3(1.0, 1.0, 1.0) : litColor;
// 	}

	//litColor = mix(litColor, vec3(1.0, 0.0, 0.0), 1.0 - (mouseDist));

	//out_color = vec4(litColor, 1.0);
	//out_color = inout_color;
	out_color = vec4(litColor, 1.0);
	//out_color = vec4(inout_uv, 0.0, 1.0);
	//out_color = vec4(rand(inout_cellCoord.x), rand(inout_cellCoord.y), rand(dot(inout_cellCoord, inout_cellCoord)), 1.0);
}
