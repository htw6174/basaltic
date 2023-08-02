#version 430

// toggle for a more mobile-friendly version of this shader
#define LIGHTWEIGHT

//precision mediump float;

#define PI 3.14159

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

uniform float time;
uniform vec2 mousePosition;
uniform int chunkIndex;

uniform sampler2DShadow shadowMap;
//uniform sampler2D shadowMap;

layout(std430, binding = 0) buffer feedbackBuffer {
	int hoveredX;
	int hoveredY;
} FeedbackBuffer;

in vec4 inout_color;
in vec3 inout_pos;
in vec3 inout_normal;
in vec4 inout_light_proj_pos;
in float inout_radius; // approximate distance from center of cell, based on vert barycentric. == 1 at cell corners, == 0.75 at center of edge
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

/*
 * Screen-space stable threshold from position hash, as described in [Wyman 2017](https://www.casual-effects.com/research/Wyman2017Hashed/index.html)
 * hash21 and hash31 from McGuire 2016, stableHash from Wyman
*/
float hash21(vec2 x) {
	return fract( 1.0e4 * sin( 17.0*x.x + 0.1*x.y ) *
		( 0.1 + abs( sin( 13.0*x.y + x.x )))
	);
}

float hash31(vec3 x) {
	return hash21( vec2( hash21( x.xy ), x.z ) );
}

// TODO: should place a limit on frequency; looks good from far away but too noisy close up
float stableHash(in vec3 pos) {
	const float g_HashScale = 1.0;

	// Find the discretized derivatives of our coordinates
	float maxDeriv = max(length(dFdx(pos)),
						 length(dFdy(pos)));
	// TEST: limit frequency with a minimum deriv
	maxDeriv = max(maxDeriv, 0.5);
	float pixScale = 1.0/(g_HashScale*maxDeriv);
	// Find two nearest log-discretized noise scales
	vec2 pixScales = vec2( exp2(floor(log2(pixScale))),
						   exp2(ceil(log2(pixScale))) );
	// Compute alpha thresholds at our two noise scales
	vec2 alpha=vec2(hash31(floor(pixScales.x * pos)),
					hash31(floor(pixScales.y * pos)));
	// Factor to interpolate lerp with
	float lerpFactor = fract( log2(pixScale) );
	// Interpolate alpha threshold from noise at two scales
	float x = (1-lerpFactor)*alpha.x + lerpFactor*alpha.y;
	// Pass into CDF to compute uniformly distrib threshold
	float a = min( lerpFactor, 1-lerpFactor );
	vec3 cases = vec3( x*x/(2*a*(1-a)),
					   (x-0.5*a)/(1-a),
					   1.0-((1-x)*(1-x)/(2*a*(1-a))) );
	// Find our final, uniformly distributed alpha threshold
	float at = (x < (1-a)) ?
	((x < a) ? cases.x : cases.y) :
	cases.z;
	// Avoids ατ == 0. Could also do ατ =1-ατ
	at = clamp( at , 1.0e-6, 1.0 );

	return at;
}

vec3 randColor(float seed) {
	return vec3(rand(seed), rand(seed + 13.0), rand(seed + 37.0));
}

// NOTE: the glsl built-in noise functions do nothing. Naming convention for my functions: noise[input dimensions][output dimensions][type]
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
	// NOTE: frag shader derivative normals always give 'flat' shading
	vec3 flatNormal = normalize(cross(dFdx(inout_pos), dFdy(inout_pos)));
	//float cliff = 1.0 - normal.z;
	bool isCliff = /*(flatNormal.z < sin(PI / 4)) &&*/ (inout_normal.z < sin(PI / 3.75));

	vec3 vN = isCliff ? flatNormal : inout_normal;

	//float normMix = smoothstep();
	//vec3 vN = mix(flatNormal, inout_normal, inout_normal.z);

	// Sample color
	float biotemp = inout_color.r;
	float understory = inout_color.g; // == % of tile with low vegetation coverage
	float canopy = inout_color.b; // == % of tile with tall vegetation coverage
	float humidityPref = inout_color.a;

	// TODO: pick from colormap
	const vec3 dirt_cold_dry = vec3(0.6, 0.6, 0.5);
	const vec3 dirt_hot_dry = vec3(1.0, 0.9, 0.5);
	const vec3 dirt_cold_wet = vec3(0.4, 0.25, 0.1);
	const vec3 dirt_hot_wet = vec3(0.5, 0.25, 0.0);

	const vec3 grass_cold_dry = vec3(0.55, 0.15, 0.2);
	const vec3 grass_hot_dry = vec3(0.6, 0.6, 0.1);
	const vec3 grass_cold_wet = vec3(0.4, 0.4, 0.2);
	const vec3 grass_hot_wet = vec3(0.35, 0.5, 0.1);

	const vec3 canopy_cold_dry = vec3(0.2, 0.25, 0.2);
	const vec3 canopy_hot_dry = vec3(0.25, 0.4, 0.2);
	const vec3 canopy_cold_wet = vec3(0.05, 0.25, 0.1);
	const vec3 canopy_hot_wet = vec3(0.05, 0.5, 0.1);

	vec3 dirtColor = mix(
		mix(dirt_cold_dry, dirt_hot_dry, biotemp),
		mix(dirt_cold_wet, dirt_hot_wet, biotemp),
		humidityPref
	);
	//vec3 grassColor = vec3(mix(0.7, 0.2, biotemp), humidityPref + 0.2, humidityPref * 0.33);
	vec3 grassColor = mix(
		mix(grass_cold_dry, grass_hot_dry, biotemp),
		mix(grass_cold_wet, grass_hot_wet, biotemp),
		humidityPref
	);
	vec3 treeColor = mix(
		mix(canopy_cold_dry, canopy_hot_dry, biotemp),
		mix(canopy_cold_wet, canopy_hot_wet, biotemp),
		humidityPref
	);
	vec3 snowColor = vec3(0.9);

#ifdef LIGHTWEIGHT
	// fill in grass from edges
	float grassThreshold = 1.0 - inout_radius;
	// expand tree cluster from center
	float treeThreshold = inout_radius;
#else
	// hashed stochastic test for vegetation coverage
	float hashThreshold = stableHash(inout_pos * 100.0);
	float grassThreshold = hashThreshold;
	float treeThreshold = hashThreshold;
#endif

	vec3 groundColor = understory < grassThreshold ? dirtColor : grassColor;

	float snowLine = step(0.15, biotemp);
	vec3 biomeColor = mix(snowColor, groundColor, snowLine);

	biomeColor = canopy < treeThreshold ? biomeColor : treeColor;

	vec3 cliffColor = vec3(0.6, 0.5, 0.5);

	float noiseFreq = isCliff ? 20.0 : 4.0;
	float noiseMag = isCliff ? 0.1 : 0.5;
	float bandFreq = isCliff ? 20.0 : 2.0;

	//float noisyZ = inout_pos.z + (sin(inout_pos.x * 2.0) * 0.2);
#ifdef LIGHTWEIGHT
	float noisyZ = inout_pos.z;
#else
	float noisyZ = inout_pos.z + (noise21sharp(inout_pos.xy * noiseFreq) * noiseMag);
#endif
	//float noisyZ = inout_pos.z + (fbm(inout_pos.xy * 20.0, 1.0) * 0.1);
	float strataSample = floor(noisyZ * bandFreq);
	float cliffValue = (rand(strataSample) * 0.2) + 0.4;
	vec3 albedo = isCliff ? cliffValue * cliffColor : biomeColor * (1.0 + (cliffValue - 0.5));

	// TODO: proper water level recoloring
	float waveHeight = sin((inout_pos.x * 5.0) + time) * 0.01;
	float elevation = inout_pos.z - waveHeight;
	if (elevation < -0.02) {
		vec3 oceanFloor = vec3(0.5, 0.3, 0.1) / -(elevation - 1.0);
		vec3 oceanSurface = vec3(0.05, 0.2, 0.9);
		albedo = mix(oceanFloor, oceanSurface, 0.5);
		vN = vec3(0.0, 0.0, 1.0);
	}

	// TEST: color per tile with obvious faces
	//albedo = randColor(rand2(inout_cellCoord));
	//vN = flatNormal;

	// Apply lighting
	vec3 lightPos = inout_light_proj_pos.xyz / inout_light_proj_pos.w;
	//vec3 shadowMapPos = vec3((lightPos.xy + 1.0) * 0.5, lightPos.z);
	vec3 shadowMapPos = (lightPos + 1.0) * 0.5;

	float shadow = texture(shadowMap, shadowMapPos.xyz);
	//float shadow = texture(shadowMap, shadowMapPos.xy).x;
	vec3 litColor = albedo * shadow;// phong(vN, normalize(vec3(1.5, -3.0, 3.0)));
	//vec3 litColor = shadow.xxx;
	//float dist = fract((shadow - lightPos.z) * 1000.0 + time);
	//float dist = shadow - ((lightPos.z + 1.0) * 0.5);
	//vec3 litColor = dist > 0.0 ? vec3(dist, 0.0, 0.0) : vec3(0.0, 0.0, 1.0 - dist);

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
