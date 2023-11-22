#version 300 es
precision mediump float;

#define PI 3.14159
#define TAU 6.28318530718

//#include "uniforms.h"

uniform float time;
uniform vec2 mousePosition;

in vec4 inout_data1;
in vec4 inout_data2;
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

// Caustics effect which tiles every 1 uv
// Based on https://www.shadertoy.com/view/MdlXz8 by David Hoskins, original by joltz0r
// FIXME: works pretty well, but with current settings (iters 3, only first 2 of final adjustments) tracks hardly show up at all below 1/2 track coverage
#define CAUSTIC_ITERS 3
float caustic(vec2 uv, float time) {
	vec2 p = mod(uv*TAU, TAU)-250.0;
	vec2 i = vec2(p);
	float c = 1.0;
	float inten = 0.005;

	for (int n = 0; n < CAUSTIC_ITERS; n++)
	{
		float t = time * (1.0 - (3.5 / float(n+1)));
		i = p + vec2(cos(t - i.x) + sin(t + i.y), sin(t - i.y) + cos(t + i.x));
		c += 1.0/length(vec2(p.x / (sin(i.x+t)/inten),p.y / (cos(i.y+t)/inten)));
	}
	c /= float(CAUSTIC_ITERS);
	c = 1.17-pow(c, 1.4);
	//c = pow(abs(c), 8.0);
	return c;
}

void main()
{
	out_id = inout_cellCoord;

	// compute normal from position deriviatives
	// NOTE: frag shader derivative normals always give 'flat' shading
	//vec3 flatNormal = normalize(cross(dFdx(inout_pos), dFdy(inout_pos)));
	//vN = flatNormal;
	bool isCliff = (inout_normal.z < sin(PI / 3.75));
	//vec3 vN = isCliff ? flatNormal : inout_normal; // if flat shaded cliff edeges are desired, else:
	vec3 vN = inout_normal;

	//float normMix = smoothstep();
	//vec3 vN = mix(flatNormal, inout_normal, inout_normal.z);

	// packed data names
	float visibility = inout_data1.x;
	float geology = inout_data1.y;
	float understory = inout_data1.z; // == % of tile with low vegetation coverage
	float canopy = inout_data1.w; // == % of tile with tall vegetation coverage

	float tracks = inout_data2.x;
	float humidityPref = inout_data2.y;
	float surfacewater = inout_data2.z;
	float biotemp = inout_data2.w;

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
	vec3 trackColor = dirtColor * 1.1;
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

	// grass cover
	vec3 biomeColor = understory > grassThreshold ? grassColor : dirtColor;

	// TODO: remove some vegetation based on tracks

	// For simple rock color grading, start with slightly boosted red&blue then adjust +/- 0.05
	vec3 cliffColor = vec3(0.4, 0.35, 0.4);

	float noiseFreq = isCliff ? 20.0 : 2.0;
	float bandFreqLow = 1.0;
	float bandFreqHigh = 20.0;

	// Reusable noise samples
	float noiseLowFreq = noise21sharp(inout_pos.xy * noiseFreq * 0.01);
	float noiseMidFreq = noise21sharp(inout_pos.xy * noiseFreq);
	float noiseHighFreq = noise21sharp(inout_pos.xy * noiseFreq * 10.0);

	// snow
	float snowReduceCoverage = noiseMidFreq * 0.25 - 0.25;
	const float biotempFreezing = 0.5;
	biomeColor = biotemp - snowReduceCoverage < biotempFreezing ? snowColor : biomeColor;

	// tracks
	float trackSample = caustic(inout_pos.xy, inout_pos.z);
	biomeColor = tracks > trackSample ? trackColor : biomeColor;

	// trees
	biomeColor = canopy > treeThreshold ? treeColor : biomeColor;

	// TODO: try out fbm for a different effect
	//float noiseMidFreq = inout_pos.z + (fbm(inout_pos.xy * 20.0, 1.0) * 0.1);
	float strataNoise = (noiseMidFreq * 0.1) + (noiseLowFreq * 20.0);
	// Bands of random value between 0.9 and 1.1
	float strataSample = floor(inout_pos.z + (strataNoise * bandFreqHigh));
	float rockLayerSample = floor(inout_pos.z * 0.01 + (strataNoise * bandFreqHigh));
	float strataValue = (rand(strataSample) * 0.4) + 0.8;
	float rockLayerRed = rand(rockLayerSample) * 0.2 - 0.1;
	float rockLayerBlue = -rockLayerRed; //rand(rockLayerSample + 69.0) * 0.2 - 0.1;

	vec3 cliffValue = vec3(rockLayerRed, 0.0, rockLayerBlue) + strataValue;

	float groundSample = floor(inout_pos.z * 5.0 + noiseMidFreq * 2.0);
	float groundValue = (rand(groundSample) * 0.2) + 0.9;

	vec3 albedo = isCliff ? cliffColor * cliffValue : biomeColor * groundValue;

	// TODO: proper water level recoloring
	float tideMag = 0.005;
	float tideFreq = 5.0;
	float tide = sin((noiseMidFreq * tideFreq) + time);

	float seaLevelOffset = 0.05;
	float isOcean = step(inout_pos.z + seaLevelOffset, tide * tideMag);

	float depth = 1.0 - min(-inout_pos.z, 1.0);
	depth = depth * depth * depth;
	vec3 oceanDeep = vec3(0.05, 0.05, 0.5);
	vec3 oceanShallow = vec3(0.05, 0.5, 0.9);
	vec3 oceanColor = mix(oceanDeep, oceanShallow, depth);

	float waveMag = 1.0;
	float waveFreq = 20.0;
	float wave = sin((noiseHighFreq * waveFreq) + time) * waveMag;
	float isWave = step(0.5, wave);
	vec3 waveCrest = vec3(0.9, 0.9, 1.0);
	//oceanColor = mix(oceanColor, waveCrest, isWave);

	vec3 seaFoam = vec3(0.8, 1.0, 1.0);
	float isFoam = step(tide * tideMag, inout_pos.z + 0.07);
	oceanColor = mix(oceanColor, seaFoam, isFoam);

	albedo = mix(albedo, oceanColor, isOcean);

	vec3 oceanNormal = vec3(0.0, 0.0, 1.0);
	vN = mix(vN, oceanNormal, isOcean);

	// non-visible terrain fades to transparent
	// TODO: use noise adjusted step instead
	float fadeout = smoothstep(0.5, 1.0, visibility);
	// half-visible terrain fades to darkened grayscale
	float visible = smoothstep(1.0, 1.9, visibility);
	float grayscale = (albedo.r + albedo.g + albedo.b) / 6.0;
	albedo = mix(vec3(grayscale), albedo, visible);

	// TEST: color per tile
	//albedo = randColor(hash21(vec2(inout_cellCoord)));

	//out_color = vec4(vec3(inout_data2.x), fadeout);
	out_color = vec4(albedo, fadeout);
	//out_color = vec4(fract(inout_pos * 10.0), 1.0);
	//out_color = vec4(vec3(hashThreshold), 1.0);
	//out_color = inout_color;
	//out_color = vec4(inout_uv, 0.0, 1.0);

	out_normal = vN;
}
