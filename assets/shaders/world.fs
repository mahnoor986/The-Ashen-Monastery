#version 330
// world.fs - The Ashen Monastery fragment shader: per-pixel lighting from up to 16 nearby lights
// (candles, sconces, lanterns, fireplaces, moonlit windows), a soft light around the player, the
// red lightning flash, a cold ambient with a little sky/ground difference, and exponential-squared
// fog toward deep indigo. Polished stone floors get a faint highlight.

#define MAX_LIGHTS 16

in vec3 fragPos;
in vec3 fragNormal;
noperspective in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3  viewPos;       // camera position
uniform vec3  fogColor;
uniform float fogDensity;
uniform float ambient;       // base light level
uniform vec3  ambientTint;   // colour of the ambient light (cold moonlight blue)
uniform float extraAmbient;  // characters: a little extra so silhouettes stay readable

uniform int   lightCount;
uniform vec3  lightsPos[MAX_LIGHTS];
uniform vec3  lightsColor[MAX_LIGHTS];   // already includes flicker and fading
uniform float lightsRadius[MAX_LIGHTS];

uniform vec3  lightPos;      // soft light around the player
uniform float lightRadius;
uniform vec3  lightColor;
uniform vec3  flashPos;      // red lightning flash (point light)
uniform vec3  flashColor;
uniform float flashRadius;
uniform float specular;      // 0..1 polished stone highlight
uniform float emissive;      // 1 = ignore lighting (glowing things), 2 = also ignore fog (eyes)
uniform float emissiveBoost; // > 1 while lightning flashes through the stained glass

out vec4 finalColor;

// Lambert diffuse with a smooth falloff to zero at the radius (+ optional highlight)
vec3 PointLight(vec3 pos, vec3 col, float radius, vec3 n, vec3 v)
{
    vec3 d = pos - fragPos;
    float dist = length(d);
    float x = dist / max(radius, 0.001);
    float f = clamp(1.0 - x * x, 0.0, 1.0);                // smooth: wide bright pool, soft edge
    vec3 l = d / max(dist, 0.0001);
    float lambert = max(dot(n, l), 0.0) * 0.8 + 0.2;     // wrap a little so edges are not pitch black
    vec3 c = col * f * f * lambert;
    if (specular > 0.0) {
        vec3 h = normalize(l + v);
        c += col * f * f * pow(max(dot(n, h), 0.0), 28.0) * specular;
    }
    return c;
}

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    vec3 n = normalize(fragNormal);
    vec3 v = normalize(viewPos - fragPos);
    if (!gl_FrontFacing) n = -n;

    float sky = 0.8 + 0.2 * n.y;                          // ceilings a bit darker, floors a bit lighter
    vec3 light = (ambient * sky + extraAmbient) * ambientTint;
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i >= lightCount) break;
        light += PointLight(lightsPos[i], lightsColor[i], lightsRadius[i], n, v);
    }
    light += PointLight(lightPos, lightColor, lightRadius, n, v);
    light += PointLight(flashPos, flashColor, flashRadius, n, v);

    vec3 color = texel.rgb * colDiffuse.rgb * mix(light, vec3(emissiveBoost), min(emissive, 1.0));

    float dist = distance(fragPos, viewPos);
    float f = fogDensity * dist;
    float fog = emissive > 1.5 ? 0.0 : clamp(1.0 - exp(-f * f), 0.0, 1.0);

    finalColor = vec4(mix(color, fogColor, fog), texel.a * colDiffuse.a);
}
