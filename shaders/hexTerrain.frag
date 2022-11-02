#version 450

precision mediump float;

#define SPECULAR 0.5
#define DIFFUSE 0.7
#define REFLECTION 1.0
#define SHININESS 10.0

#define LIGHT_AMBIENT 0.1
#define LIGHT1_SPECULAR 1.0
#define LIGHT1_DIFFUSE 1.0

// Distance where fog starts to fade in
#define FOG_MIN 48.0
// Distance where only fog is visible
#define FOG_MAX 64.0
#define FOG_COLOR vec3(0.1, 0.1, 0.2)

// NOTE: because we only want to write to the feedback buffer from visible fragments, early depth testing is required
layout(early_fragment_tests) in;

layout(set = 0, binding = 0) uniform windowInfo {
	vec2 windowSize;
	vec2 mousePosition;
	vec3 cameraPosition;
	vec3 cameraFocalPoint;
} WindowInfo;

layout(std430, set = 0, binding = 1) buffer feedbackBuffer {
	uint hoveredChunk;
	uint hoveredCell;
} FeedbackBuffer;

layout(location = 0) in vec3 in_color;
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

float fog() {
	float worldDistance = distance(in_pos, WindowInfo.cameraFocalPoint);
	return smoothstep(FOG_MIN, FOG_MAX, worldDistance);
}

void main()
{
	// compute normal from position deriviatives
	vec3 normal = normalize(cross(dFdy(in_pos), dFdx(in_pos)));

	vec2 windowPos = gl_FragCoord.xy;
	vec2 mousePos = WindowInfo.mousePosition;
	float mouseDist = distance(windowPos, mousePos);
	if (mouseDist < 1.0) {
		FeedbackBuffer.hoveredChunk = in_chunkIndex;
		FeedbackBuffer.hoveredCell = in_cellIndex;
	}
	vec3 litColor = in_color * phong(normal, normalize(vec3(1.5, -3.0, 3.0)));
	//litColor = mix(litColor, FOG_COLOR, fog()); // TODO: why not just use 1-fog for alpha?
	out_color = vec4(litColor, 1.0);
	//out_color = vec4(in_color, 1.0);
	//out_color = vec4(normal, 1.0);
}
