#version 300 es
precision mediump float;

uniform mat4 pv;
uniform mat4 m;

layout(location = 0) in vec3 in_start;
layout(location = 1) in vec3 in_end;
layout(location = 2) in vec4 in_color;
layout(location = 3) in float in_width;
layout(location = 4) in float in_speed;

out vec2 inout_uv;
out vec4 inout_color;

vec3 quadVerts[4] = vec3[](
    // start, clockwise from bottom-right
    vec3(0.5, 0.0, 0.0),
    vec3(-0.5, 0.0, 0.0),
    // end
    vec3(-0.5, 1.0, 0.0),
    vec3(0.5, 1.0, 0.0)
);

vec2 quadUvs[4] = vec2[](
    // start
    vec2(1.0, 0.0),
    vec2(0.0, 0.0),
    // end
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

int quadIndicies[6] = int[](
    0, 1, 3,
    2, 3, 1
);

void main()
{
    vec3 vert = quadVerts[quadIndicies[gl_VertexID]];
    vec3 toEnd = in_end - in_start;
    vec3 right = normalize(cross(toEnd, vec3(0.0, 0.0, 1.0)));
    vec3 pos = (right * in_width * vert.x) + (toEnd * vert.y);
    gl_Position = pv * m * vec4(in_start + pos, 1.0);

    inout_uv = quadUvs[quadIndicies[gl_VertexID]];
    inout_uv.y *= length(toEnd) * (1.0 / in_width);
    inout_color = in_color;
}

