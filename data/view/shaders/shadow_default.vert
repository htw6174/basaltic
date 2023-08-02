#version 330

precision mediump float;

uniform mat4 pv;
uniform mat4 m;

layout(location = 0) in vec4 in_position;

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
    vec3 vert = octahedronPositions[octahedronIndicies[gl_VertexID]];
    gl_Position = pv * m * vec4(in_position.xyz + vert, 1.0);
}

