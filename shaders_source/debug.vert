#version 450

precision mediump float;

#include "uniforms.h"

layout(push_constant) uniform matPV {
    mat4 pv;
    mat4 m;
} MPV;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec4 in_position;
layout(location = 2) in float in_size;

layout(location = 0) out vec4 out_color;

vec3 octahedronPositions[6] = vec3[](
    // bottom
    vec3(0.0, 0.0, -0.5),
    // middle
    vec3(0.0, 0.5, 0.0),
    vec3(0.5, 0.0, 0.0),
    vec3(0.0, -0.5, 0.0),
    vec3(-0.5, 0.0, 0.0),
    // top
    vec3(0.0, 0.0, 0.5)
);

int octahedronIndicies[24] = int[](
    // bottom half
    2, 1, 0, // north east, runs clockwise
    3, 2, 0,
    4, 3, 0,
    1, 4, 0,
    // top half
    1, 2, 5,
    2, 3, 5,
    3, 4, 5,
    4, 1, 5
);

void main()
{
    vec3 vert = octahedronPositions[octahedronIndicies[gl_VertexIndex]];
    vec2 xy = in_position.xy + vert.xy;
    float z = (in_position.z * WorldInfo.gridToWorld.z) + (vert.z * in_size);
    gl_Position = MPV.pv * vec4(xy, z, 1.0) * vec4(1.0, -1.0, 1.0, 1.0); // flip y so it draws correctly for now; transform matricies should do this automatically later

    out_color = in_color;
}
