#version 300 es
precision mediump float;

// toggle for a more mobile-friendly version of this shader, saves ~3ms on laptop
//#define LIGHTWEIGHT

#define PI 3.14159

//#include "uniforms.h"

uniform float time;
uniform vec2 mousePosition;
uniform int chunkIndex;

in vec4 inout_color;
in vec3 inout_pos;
in vec3 inout_normal;
in float inout_radius; // approximate distance from center of cell, based on vert barycentric. == 1 at cell corners, == 0.75 at center of edge
flat in ivec2 inout_cellCoord;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out ivec2 out_id;

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

float stableHash(in vec3 pos) {
	const float g_HashScale = 1.0;

	// Find the discretized derivatives of our coordinates
	float maxDeriv = max(length(dFdx(pos)),
						 length(dFdy(pos)));
	// TEST: limit frequency with a minimum deriv
	//maxDeriv = max(maxDeriv, 0.5);
	float pixScale = 1.0/(g_HashScale*maxDeriv);
	// Find two nearest log-discretized noise scales
	vec2 pixScales = vec2( exp2(floor(log2(pixScale))),
						   exp2(ceil(log2(pixScale))) );
	// Compute alpha thresholds at our two noise scales
	// NOTE: swizzle here is to reduce precision issues with large x, y, and pixScale i.e. far from origin and close to camera
	vec2 alpha=vec2(hash31(floor(pixScales.x * pos.zyx)),
					hash31(floor(pixScales.y * pos.zyx)));
	// Factor to interpolate lerp with
	float lerpFactor = fract( log2(pixScale) );
	// Interpolate alpha threshold from noise at two scales
	float x = (1.0-lerpFactor)*alpha.x + lerpFactor*alpha.y;
	// Pass into CDF to compute uniformly distrib threshold
	float a = min( lerpFactor, 1.0-lerpFactor );
	vec3 cases = vec3( x*x/(2.0*a*(1.0-a)),
					   (x-0.5*a)/(1.0-a),
					   1.0-((1.0-x)*(1.0-x)/(2.0*a*(1.0-a))) );
	// Find our final, uniformly distributed alpha threshold
	float at = (x < (1.0-a)) ?
	((x < a) ? cases.x : cases.y) :
	cases.z;
	// Avoids ατ == 0. Could also do ατ =1-ατ
	// NOTE: original paper cuts off small values becaues they don't make sense for alpha testing, but this causes artifacts near camera
	//at = clamp( at , 1.0e-6, 1.0 );

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

// Fractal Brownian Motion, Based on IQ's article - https://iquilezles.org/articles/fbm/
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

void main()
{
	out_id = inout_cellCoord;

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
	// NOTE: stableHash breaks down when close to the camera and far from origin. Scaling world position down may help
	// Mix to nearHash when close to camera if lower frequency noise is desired
	float farHash = stableHash(inout_pos);
	//float nearHash = hash31(inout_pos * 1.0);
	float hashThreshold = farHash;
	float grassThreshold = hashThreshold;
	float treeThreshold = hashThreshold;
#endif

	vec3 groundColor = understory < grassThreshold ? dirtColor : grassColor;

	float snowLine = step(0.15, biotemp);
	vec3 biomeColor = mix(snowColor, groundColor, snowLine);

	biomeColor = canopy < treeThreshold ? biomeColor : treeColor;

	vec3 cliffColor = vec3(0.3, 0.25, 0.25);

	float noiseFreq = isCliff ? 20.0 : 4.0;
	float bandFreq = isCliff ? 20.0 : 2.0;

	// Reusable noise samples
	float noiseLowFreq = noise21sharp(inout_pos.xy * noiseFreq * 0.1);
	float noiseMidFreq = noise21sharp(inout_pos.xy * noiseFreq);
	float noiseHighFreq = noise21sharp(inout_pos.xy * noiseFreq * 10.0);

	// TODO: try out fbm for a different effect
	//float noiseMidFreq = inout_pos.z + (fbm(inout_pos.xy * 20.0, 1.0) * 0.1);
	float strataNoise = (noiseMidFreq * 0.1) + (noiseLowFreq * 2.0);
	// Bands of random value between 0.9 and 1.1
	float strataSample = floor(inout_pos.z + (strataNoise * bandFreq));
	float cliffValue = (rand(strataSample) * 0.2) + 0.9;

	float groundSample = floor(inout_pos.z + noiseMidFreq * bandFreq);
	float groundValue = (rand(groundSample) * 0.2) + 0.9;

	vec3 albedo = isCliff ? cliffColor * cliffValue : biomeColor * groundValue;

	// TODO: proper water level recoloring
	float waveMag = 0.005;
	float waveFreq = 5.0;
	float wave = sin((noiseMidFreq * waveFreq) + time);

	float seaLevelOffset = -0.05;
	float seaLevel = inout_pos.z + seaLevelOffset;
	float isOcean = step(seaLevel, wave * waveMag);

	float depth = 1.0 - min(-inout_pos.z, 1.0);
	depth = depth * depth * depth;
	vec3 oceanDeep = vec3(0.05, 0.05, 0.5);
	vec3 oceanShallow = vec3(0.05, 0.5, 0.9);
	vec3 oceanColor = mix(oceanDeep, oceanShallow, depth);

	vec3 seaFoam = vec3(0.8, 1.0, 1.0);
	float isFoam = step(wave * waveMag, seaLevel + 0.02);
	oceanColor = mix(oceanColor, seaFoam, isFoam);

	albedo = mix(albedo, oceanColor, isOcean);

	vec3 oceanNormal = vec3(0.0, 0.0, 1.0);
	vN = mix(vN, oceanNormal, isOcean);

	// TEST: color per tile with obvious faces
	//albedo = randColor(hash21(vec2(inout_cellCoord)));
	//vN = flatNormal;

	out_color = vec4(albedo, 1.0);
	//out_color = vec4(dFdy(inout_pos) * 100.0, 1.0);
	//out_color = vec4(fract(inout_pos * 10.0), 1.0);
	//out_color = vec4(vec3(hashThreshold), 1.0);
	//out_color = inout_color;
	//out_color = vec4(inout_uv, 0.0, 1.0);

	out_normal = vN;
}
