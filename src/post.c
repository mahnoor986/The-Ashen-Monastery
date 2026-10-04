/* post.c - low-resolution scene target + post-process shader (see post.h). */
#include <stdio.h>
#include "post.h"
#include "config.h"

#define POST_FS "assets/shaders/post.fs"

static RenderTexture2D scene;     /* module-private GPU resources */
static Shader shader;
static bool   hasShader, enabled = true;
static int    locTime, locGrade, locMode, locTexel, locDither, locGrain, locPulse;

void Post_Init(void)
{
    scene = LoadRenderTexture(POST_W, POST_H);
    SetTextureFilter(scene.texture, TEXTURE_FILTER_POINT);

    hasShader = false;
    if (FileExists(POST_FS)) {
        shader = LoadShader(NULL, POST_FS);
        hasShader = IsShaderValid(shader) && shader.id != 0;
    }
    if (!hasShader) {
        printf("warning: %s not loaded, drawing the scene without post-processing\n", POST_FS);
        return;
    }
    locTime = GetShaderLocation(shader, "time");
    locGrade = GetShaderLocation(shader, "gradeStrength");
    locMode = GetShaderLocation(shader, "gradeMode");
    locTexel = GetShaderLocation(shader, "texelSize");
    locDither = GetShaderLocation(shader, "dither");
    locGrain = GetShaderLocation(shader, "grain");
    locPulse = GetShaderLocation(shader, "pulse");
}

void Post_Shutdown(void)
{
    UnloadRenderTexture(scene);
    if (hasShader) UnloadShader(shader);
}

void Post_BeginScene(void) { BeginTextureMode(scene); }
void Post_EndScene(void)   { EndTextureMode(); }
void Post_Toggle(void)     { enabled = !enabled; }
bool Post_Enabled(void)    { return enabled; }

void Post_Draw(float time, int gradeMode, float pulse)
{
    Rectangle src = { 0, 0, (float)POST_W, -(float)POST_H };   /* render textures are upside down */
    Rectangle dst = { 0, 0, (float)SCREEN_W, (float)SCREEN_H };
    bool useShader = hasShader && enabled;

    if (useShader) {
        float grade = POST_GRADE_STRENGTH, mode = (float)gradeMode, dither = POST_DITHER ? 1.0f : 0.0f;
        float grain = POST_GRAIN;
        Vector2 texel = { 1.0f / POST_W, 1.0f / POST_H };
        SetShaderValue(shader, locTime, &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, locGrade, &grade, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, locMode, &mode, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, locTexel, &texel, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, locDither, &dither, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, locGrain, &grain, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, locPulse, &pulse, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(shader);
    }
    DrawTexturePro(scene.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    if (useShader) EndShaderMode();
    else if (pulse > 0.0f) DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 190, 5, 5, (unsigned char)(110 * pulse) });
}
