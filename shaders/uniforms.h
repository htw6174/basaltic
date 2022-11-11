// structs here must match the corresponding structs in kingdom_window.h
// everything here can be used in both the vertex and fragment stages

layout(std140, set = 0, binding = 0) uniform windowInfo {
	vec2 windowSize;
	vec2 mousePosition;
	vec3 cameraPosition;
	vec3 cameraFocalPoint;
} WindowInfo;

layout(std140, set = 0, binding = 2) uniform worldInfo {
	vec3 gridToWorld; // Conversion factors from orthogonal grid coordinates to scaled hex cell coordinates. NOTE: Only .z used currently
    float totalWidth;
    int timeOfDay;
    // season info, weather, etc.
} WorldInfo;
