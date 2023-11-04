#version 300 es
precision mediump float;
precision highp sampler2DShadow;

#define SPECULAR 0.5
#define DIFFUSE 1.25
#define REFLECTION 1.0
#define SHININESS 10.0

#define LIGHT_AMBIENT 0.2
#define LIGHT1_SPECULAR 1.0
#define LIGHT1_DIFFUSE 1.0

uniform sampler2D diffuse;
uniform sampler2D normal;
uniform sampler2DShadow depth;
uniform sampler2DShadow shadowMap;

uniform mat4 pv;
uniform mat4 sun;
uniform vec2 invZ;
uniform vec3 camera_position;
uniform vec3 toSun;
uniform mat4 inverse_view;

// TODO: sunlight and shadow colors, others

in vec2 uv;
in vec4 view_dir;
in vec4 world_dir;

out vec4 frag_color;

// Returns view-space z coord. Note that OpenGL faces down the z axis, so this should be negative, but I flip it for more predictable behavior.
// NB: invZ is really just the projection matrix [4, 3] and [3, 3]. Wouldn't be required if proj and view were supplied separately
// From David Lenaerts: https://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer/
float reconstructDepth(in float rawDepth) {
    //return -p[3][2] / ((2.0 * rawDepth - 1.0) + p[2][2]);
    return invZ.y / ((2.0 * rawDepth - 1.0) + invZ.x);
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

vec3 simpleFog(in vec3 color, in float dist) {
    float b = 0.01;
    float intensity = 1.0 - exp(-dist * b);
    vec3 fogColor = vec3(0.5, 0.6, 0.7);
    return mix(color, fogColor, intensity);
}

vec3 linearFog(in vec3 color, in vec3 fogColor, in float dist, in vec3 origin, in vec3 dir) {
    float a = 0.005;
    float b = 0.005;
    float intensity = (a / b) * exp(-origin.z * b) * (1.0 - exp(-dist * dir.z * b)) / dir.z;
    intensity = min(1.0, intensity);
    return mix(color, fogColor, intensity);
    //return vec3(intensity);
}

void main() {
    vec4 diff = texture(diffuse, uv);
    vec3 norm = texture(normal, uv).xyz;
    float dep = texture(depth, vec3(uv, 1.0));

    float viewZ = reconstructDepth(dep);
    vec4 viewPos = view_dir * viewZ;
    //vec4 worldPos = inverse_view * vec4(viewPos.xyz, 1.0);
    vec4 wd = -(world_dir - vec4(camera_position, 1.0));
    vec4 worldPos = vec4(camera_position, 1.0) + viewZ * wd;

    // Shadows and lighting TODO:
    // - Use sun params for direct and indirect light color + intensity

    // constants
    vec3 sunLightColor = vec3(1.65, 1.6, 1.1);// vec3(1.65, 1.27, 0.99); // better for sunset
    vec3 shadowColor = vec3(1.0, 1.2, 1.5);
    vec3 skyLightColor = vec3(0.16, 0.2, 0.28);

    vec3 sunWhite = vec3(1.0, 1.0, 0.8);
    vec3 skyBlue = vec3(0.4, 0.4, 0.8);
    vec3 midnightBlue = vec3(0.1, 0.1, 0.1);

    // contribution magnitudes
    float dotLN = dot(toSun, norm);
    float sunMag = max(dotLN, 0.0);
    sunMag *= step(0.0, toSun.z);
    float skyMag = max(0.5 * norm.z + 0.5, 0.0);

    // sample shadow map
    vec4 normalBias = vec4(norm * 0.05 + worldPos.xyz, worldPos.w);
    vec4 light_proj_pos = sun * normalBias;
    vec3 lightPos = light_proj_pos.xyz / light_proj_pos.w;
    vec3 shadowMapPos = (lightPos + 1.0) * 0.5;
    float shadowSample = texture(shadowMap, shadowMapPos.xyz);
    //directLight = min(directLight, dotLN);

//     float ambient = LIGHT_AMBIENT;
//     float diffuse = directLight * DIFFUSE;
//     float totalLight = ambient + diffuse;

    // accumulate lights
    vec3 lightColor = sunMag * sunLightColor * pow(vec3(shadowSample), shadowColor);
    lightColor += skyMag * skyLightColor;

    vec3 lit = (diff.rgb * 0.75) * lightColor;

    // skybox TODO: write to the diffuse target before lighting pass?
    float sunSpot = smoothstep(0.995, 1.0, dot(toSun, normalize(wd.xyz)));
    vec3 sky = mix(skyBlue, sunWhite, sunSpot);
    float horizon = smoothstep(-0.3, 0.0, wd.z);
    sky = mix(midnightBlue, sky, horizon);

    // fade into skybox at horizontal view limit
    float viewDist = 128.0;
    float horizontalDist = distance(camera_position.xy, worldPos.xy);
    //(camera_position.x * camera_position.x) + (camera_position.y * camera_position.y);
    //float skybox = smoothstep(viewDist * 0.9, viewDist, horizontalDist);
    float skybox = 1.0 - diff.a;

    //float viewEdge = smoothstep(viewDist * 0.9, viewDist, horizontalDist);
    float viewEdge = smoothstep(viewDist * 0.9, viewDist * 1.5, horizontalDist);

    // fog
    vec3 fogColor = vec3(0.5, 0.6, 0.7);
    fogColor = mix(fogColor, sky, viewEdge);
    //vec3 fogColor = sky;
    lit = linearFog(lit, fogColor, viewZ, camera_position, wd.xyz);

    // DEBUG
    //vec3 wtf = lightPos.xyz;
    vec3 wtf = fract(worldPos.xyz);
    //vec3 wtf = wd.xyz;

    //vec3 color = mix(sky, wtf, diff.a);
    vec3 color = mix(lit, sky, skybox);

    // area to draw reference values
//     if (uv.x < 0.1) {
//         float swatches = floor(uv.y * 11.0) / 10.0;
//
//         color = vec3(swatches);
//     }

    //vec3 depthVis = vec3(fract(viewZ));
    //frag_color = diffuse;// + (vec4(normal, depth) * shadow);
    //frag_color = vec4(mix(sky, diff.rgb, diff.a), 1.0);
    //frag_color = vec4(vec3(dep), 1.0);
    //frag_color = vec4(norm * 0.5 + 0.5, 1.0);
    //frag_color = vec4(wtf, 1.0);
    frag_color = vec4(color, 1.0);
}
