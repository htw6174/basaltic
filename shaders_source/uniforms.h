// structs here must match the corresponding structs in kingdom_window.h
// everything here can be used in both the vertex and fragment stages

const float PI = 3.14159f;
const float TAU = 6.28319f;

layout(std140, set = 0, binding = 0) uniform windowInfo {
	vec2 windowSize;
	vec2 mousePosition;
	vec3 cameraPosition;
	vec3 cameraFocalPoint;
} WindowInfo;

layout(std140, set = 0, binding = 2) uniform worldInfo {
	vec3 gridToWorld; // Conversion factors from orthogonal grid coordinates to scaled hex cell coordinates. NOTE: Only .z used currently
    float totalWidth;
    uint time;
    // season info, weather, etc.
	int seaLevel;
	// debug options
	uint visibilityOverrideBits;
} WorldInfo;

const uint visibilityBitGeometry = 		0x00000001;
const uint visibilityBitColor = 		0x00000002;
const uint visibilityBitMegaliths = 	0x00000004;
const uint visibilityBitVegetation = 	0x00000008;
const uint visibilityBitStructures = 	0x00000010;
const uint visibilityBitMegafauna = 	0x00000020;
const uint visibilityBitCharacters = 	0x00000040;
const uint visibilityBitSecrets = 		0x00000080;
