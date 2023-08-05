#version 330

#define SPECULAR 0.5
#define DIFFUSE 0.7
#define REFLECTION 1.0
#define SHININESS 10.0

#define LIGHT_AMBIENT 0.3
#define LIGHT1_SPECULAR 1.0
#define LIGHT1_DIFFUSE 1.0

uniform sampler2D diffuse;
uniform sampler2D normal;
uniform sampler2D depth;
uniform sampler2DShadow shadowMap;

uniform mat4 pv;
uniform mat4 sun;
uniform vec2 invZ;
uniform vec3 camera_position;
uniform vec3 toSun;

// TODO: sunlight and shadow colors, others

in vec2 uv;
in vec4 view_dir;
in vec4 world_dir;

out vec4 frag_color;

// Returns view-space z coord. Note that OpenGL faces down the z axis, i.e. this will typically be negative.
// NB: invZ is really just the projection matrix [4, 3] and [3, 3]. Wouldn't be required if proj and view were supplied separately
// From David Lenaerts: https://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer/
float reconstructDepth(in float rawDepth) {
    //return -p[3][2] / ((2.0 * rawDepth - 1.0) + p[2][2]);
    return -invZ.y / ((2.0 * rawDepth - 1.0) + invZ.x);
}

vec3 reconstructViewPos(in float z) {
    return view_dir.xyz * z;
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

void main() {
    vec4 diff = texture(diffuse, uv);
    vec3 norm = texture(normal, uv).xyz;
    float dep = texture(depth, uv).x;

    float viewZ = reconstructDepth(dep);
    vec4 viewPos = view_dir * viewZ;
    vec4 worldPos = world_dir * viewZ + vec4(camera_position, 1.0);

    vec4 light_proj_pos = sun * worldPos;
    vec3 lightPos = light_proj_pos.xyz / light_proj_pos.w;
    vec3 shadowMapPos = (lightPos + 1.0) * 0.5;

    float shadow = texture(shadowMap, shadowMapPos.xyz);

    // Shadows and lighting TODO:
    // - get sun direction vector, use for phong
    // - Where dotLN < 0, don't use shadowMap
    // - Shadows and backfacing sides should have same ambient light
    // - Use sun params for direct and indirect light color + intensity

    vec3 lit = diff.xyz * phong(norm, toSun) * shadow;

    // TODO: write to the diffuse target before other lighting?
    vec3 sky = vec3(0.5, 0.5, 0.7);

    //vec3 wtf = vec3(fract(viewZ));
    vec3 wtf = fract(viewPos.xyz);
    //vec3 wtf = world_dir.xyz;

    vec3 color = mix(sky, wtf, diff.a);
    //vec3 color = mix(sky, lit, diff.a);

    //vec3 depthVis = vec3(fract(viewZ));
    //frag_color = diffuse;// + (vec4(normal, depth) * shadow);
    //frag_color = vec4(mix(sky, diff.rgb, diff.a), 1.0);
    //frag_color = vec4(vec3(dep), 1.0);
    //frag_color = vec4(norm * 0.5 + 0.5, 1.0);
    frag_color = vec4(color, 1.0);
}
