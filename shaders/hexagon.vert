#version 320 es
/* Copyright (c) 2019, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 #define HALF_HEIGHT 0.433012701892
 // controls the direction where hexagons will run straight; left-right for ORTHO_HORIZONTAL, up-down for ORTHO_VERTICAL
 #define ORTHO_HORIZONTAL

precision mediump float;

layout(push_constant) uniform matPV {
    mat4 projection;
    mat4 view;
} MatPV;

layout(location = 0) in vec4 in_position;

layout(location = 0) out vec3 out_color;

// x,y are offsets from instance position.xy, z is scaled by instance position.z
// TODO: update to vec3, add side verts
#ifdef ORTHO_VERTICAL
vec2 hexagon_positions[7] = vec2[](
    vec2(0.0, 0.0), // center
    vec2(0.25, HALF_HEIGHT), // top right, runs clockwise
    vec2(0.5, 0.0),
    vec2(0.25, -HALF_HEIGHT),
    vec2(-0.25, -HALF_HEIGHT),
    vec2(-0.5, 0.0),
    vec2(-0.25, HALF_HEIGHT)
);
#endif

#ifdef ORTHO_HORIZONTAL
vec3 hexagon_positions[13] = vec3[](
    // Horizontal surface
    vec3(0.0, 0.0, 1.0), // center
    vec3(0.0, 0.5, 1.0), // top middle, runs clockwise
    vec3(HALF_HEIGHT, 0.25, 1.0),
    vec3(HALF_HEIGHT, -0.25, 1.0),
    vec3(0, -0.5, 1.0),
    vec3(-HALF_HEIGHT, -0.25, 1.0),
    vec3(-HALF_HEIGHT, 0.25, 1.0),
    // Sides
    vec3(0.0, 0.5, 0.0), // index 7
    vec3(HALF_HEIGHT, 0.25, 0.0), // 8
    vec3(HALF_HEIGHT, -0.25, 0.0), // 9
    vec3(0, -0.5, 0.0), // 10
    vec3(-HALF_HEIGHT, -0.25, 0.0), // 11
    vec3(-HALF_HEIGHT, 0.25, 0.0) // 12
);
#endif

int hexagon_indicies[54] = int[](
    // Horizontal surface
    0, 1, 2, // top right, runs clockwise
    0, 2, 3,
    0, 3, 4,
    0, 4, 5,
    0, 5, 6,
    0, 6, 1,
    // Sides
    1, 7, 2,
    2, 7, 8,
    2, 8, 3,
    3, 8, 9,
    3, 9, 4,
    4, 9, 10,
    4, 10, 5,
    5, 10, 11,
    5, 11, 6,
    6, 11, 12,
    6, 12, 1,
    1, 12, 7
);

vec3 hexagon_colors[7] = vec3[](
    vec3(0.5, 0.5, 0.5),
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 1.0, 1.0),
    vec3(1.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0)
);

void main()
{
    vec3 vert = hexagon_positions[hexagon_indicies[gl_VertexIndex]];
    vec2 xy = in_position.xy + vert.xy;
    float z = in_position.z * vert.z;
    gl_Position = MatPV.projection * MatPV.view * vec4(xy, z, 1.0) * vec4(1.0, -1.0, 1.0, 1.0); // flip y so it draws correctly for now; transform matricies should do this automatically later

    out_color = hexagon_colors[hexagon_indicies[gl_VertexIndex] % 7];
}
