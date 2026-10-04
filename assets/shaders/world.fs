#version 330
// world.fs - Blackthorn Manor fragment shader: baked torch light + ambient + a warm light
// around the player, then exponential-squared fog toward near-black.
//   vertex color rgb = baked torch light, vertex color alpha = face shade (top 1.0 ... bottom 0.5)

in vec3 fragPos;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3  viewPos;       // camera position
uniform vec3  fogColor;
uniform float fogDensity;
uniform float ambient;       // base light level
uniform vec3  ambientTint;   // colour of the ambient light
uniform float flicker;       // global torch flicker (about 0.88)
uniform vec3  lightPos;      // player light
uniform float lightRadius;
uniform vec3  lightColor;
uniform vec3  flashPos;      // red lightning flash (point light)
uniform vec3  flashColor;
uniform float flashRadius;
uniform vec3  entityLight;   // torch light at a character (characters have no baked light)
uniform float isEntity;      // 1 = use entityLight instead of the vertex color rgb
uniform float emissive;      // 1 = ignore lighting (glowing things)

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    float shade = fragColor.a;
    vec3 baked = mix(fragColor.rgb, entityLight, isEntity);

    float d = distance(fragPos, lightPos);
    float p = clamp(1.0 - d / lightRadius, 0.0, 1.0);
    vec3 playerLight = lightColor * p * p;

    float fd = clamp(1.0 - distance(fragPos, flashPos) / max(flashRadius, 0.001), 0.0, 1.0);
    vec3 flash = flashColor * fd * fd;

    vec3 light = (ambient * ambientTint + baked * flicker + playerLight + flash) * shade;
    vec3 color = texel.rgb * colDiffuse.rgb * mix(light, vec3(1.0), emissive);

    float dist = distance(fragPos, viewPos);
    float f = fogDensity * dist;
    float fog = clamp(1.0 - exp(-f * f), 0.0, 1.0);

    finalColor = vec4(mix(color, fogColor, fog), texel.a * colDiffuse.a);
}
