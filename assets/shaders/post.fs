#version 330
// post.fs - The Ashen Monastery post-process (fragment only; raylib supplies the vertex shader).
// The 3D scene is rendered at 640x360 and drawn up-scaled through this shader:
//   colour grade (0 = moonlit gothic split-tone, 1 = warm gold), mild contrast,
//   chromatic offset, film grain, vignette, red pulse, optional PS1 5-bit colour + 4x4 dither.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4  colDiffuse;

uniform float time;
uniform float gradeStrength;   // 0 = original colours, 1 = fully graded
uniform float gradeMode;       // 0 = moonlit gothic, 1 = warm gold (Sanctum)
uniform vec2  texelSize;       // size of one low-res pixel in UV units
uniform float dither;          // 1 = PS1 colour reduction
uniform float grain;           // film grain strength
uniform float pulse;           // 0..1 red flash (a bell shatters)

out vec4 finalColor;

const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
                                  3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 uv = fragTexCoord;

    // subtle chromatic offset: red and blue sampled one low-res pixel apart
    vec3 c;
    c.r = texture(texture0, uv + vec2(texelSize.x, 0.0)).r;
    c.g = texture(texture0, uv).g;
    c.b = texture(texture0, uv - vec2(texelSize.x, 0.0)).b;

    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    vec3 graded;
    if (gradeMode < 0.5) {
        // moonlit gothic: split-tone (cool blue-indigo shadows, warm amber highlights),
        // slightly lowered saturation, mild contrast; deep shadows lift to indigo, never pure black
        vec3 desat = mix(vec3(lum), c, 0.82);
        vec3 shadowTint = vec3(0.80, 0.90, 1.22);
        vec3 highTint   = vec3(1.12, 1.00, 0.82);
        float h = smoothstep(0.10, 0.65, lum);
        graded = desat * mix(shadowTint, highTint, h);
        graded += vec3(0.010, 0.014, 0.032) * (1.0 - h);
    } else {
        // warm gold: candle-lit amber (the Sanctum)
        vec3 desat = mix(vec3(lum), c, 0.85);
        graded = desat * vec3(1.18, 1.0, 0.72) + vec3(0.025, 0.015, 0.0);
    }
    vec3 col = mix(c, graded, gradeStrength);

    // mild contrast (S-curve around mid grey)
    col = clamp(col, 0.0, 1.0);
    col = mix(col, col * col * (3.0 - 2.0 * col), 0.35);

    // red pulse
    col = mix(col, vec3(0.75, 0.02, 0.02), pulse * 0.45);

    // vignette (soft, toward deep indigo instead of black)
    float d = length((uv - 0.5) * vec2(1.25, 1.0));
    col = mix(vec3(0.004, 0.006, 0.016), col, mix(0.35, 1.0, smoothstep(0.85, 0.3, d)));

    // film grain (animated)
    col += (hash(gl_FragCoord.xy + fract(time) * 517.0) - 0.5) * grain;

    // PS1 colour reduction: 5 bits per channel with a 4x4 ordered dither on low-res pixels
    if (dither > 0.5) {
        ivec2 p = ivec2(mod(floor(gl_FragCoord.xy * 0.5), 4.0));
        float t = bayer[p.y * 4 + p.x] / 16.0 - 0.5;
        col = floor(clamp(col, 0.0, 1.0) * 31.0 + 0.5 + t) / 31.0;
    }

    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
