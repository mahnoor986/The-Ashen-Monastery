/* render.c - world shader setup, per-frame uniforms and world drawing (see render.h). */
#include <stdio.h>
#include <math.h>
#include "render.h"
#include "textures.h"
#include "meshgen.h"
#include "raymath.h"

#define SHADER_VS "assets/shaders/world.vs"
#define SHADER_FS "assets/shaders/world.fs"

static Shader   shader;       /* module-private GPU resources */
static bool     hasShader;
static Material worldMat;
static int locFogColor, locFogDensity, locAmbient, locFlicker, locLightPos, locLightRadius,
           locLightColor, locEntityLight, locIsEntity, locEmissive;

void Render_Init(void)
{
    hasShader = false;
    if (FileExists(SHADER_VS) && FileExists(SHADER_FS)) {
        shader = LoadShader(SHADER_VS, SHADER_FS);
        hasShader = IsShaderValid(shader) && shader.id != 0;
    }
    if (hasShader) {
        shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
        shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
        locFogColor = GetShaderLocation(shader, "fogColor");
        locFogDensity = GetShaderLocation(shader, "fogDensity");
        locAmbient = GetShaderLocation(shader, "ambient");
        locFlicker = GetShaderLocation(shader, "flicker");
        locLightPos = GetShaderLocation(shader, "lightPos");
        locLightRadius = GetShaderLocation(shader, "lightRadius");
        locLightColor = GetShaderLocation(shader, "lightColor");
        locEntityLight = GetShaderLocation(shader, "entityLight");
        locIsEntity = GetShaderLocation(shader, "isEntity");
        locEmissive = GetShaderLocation(shader, "emissive");
    } else {
        printf("warning: world shader not loaded, using raylib's default shader (no fog)\n");
    }
    MB_SetShaderEncoding(hasShader);

    worldMat = LoadMaterialDefault();
    worldMat.maps[MATERIAL_MAP_DIFFUSE].texture = Textures_Atlas();
    if (hasShader) worldMat.shader = shader;
}

void Render_Shutdown(void)
{
    worldMat.maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ 0 };   /* owned by textures.c */
    worldMat.shader = (Shader){ 0 };                                  /* unloaded below */
    UnloadMaterial(worldMat);
    if (hasShader) UnloadShader(shader);
}

bool Render_HasShader(void) { return hasShader; }
Material Render_WorldMaterial(void) { return worldMat; }

Shader Render_Shader(void)
{
    return hasShader ? shader : worldMat.shader;
}

float Render_Flicker(float time)
{
    return 0.88f + 0.05f * sinf(time * 7.3f) + 0.03f * sinf(time * 13.1f + 1.3f) + 0.02f * sinf(time * 23.7f);
}

static void SetF(int loc, float v)    { SetShaderValue(shader, loc, &v, SHADER_UNIFORM_FLOAT); }
static void SetV3(int loc, Vector3 v) { SetShaderValue(shader, loc, &v, SHADER_UNIFORM_VEC3); }

void Render_BeginFrame(const WingConfig *wing, Camera3D cam, Vector3 playerPos, float time)
{
    if (!hasShader) return;
    SetV3(shader.locs[SHADER_LOC_VECTOR_VIEW], cam.position);
    SetV3(locFogColor, (Vector3){ FOG_COLOR_R, FOG_COLOR_G, FOG_COLOR_B });
    SetF(locFogDensity, wing->fogDensity);
    SetF(locAmbient, wing->ambient);
    SetF(locFlicker, Render_Flicker(time));
    SetV3(locLightPos, (Vector3){ playerPos.x, playerPos.y + PLAYER_LIGHT_HEIGHT, playerPos.z });
    SetF(locLightRadius, wing->playerLightRadius);
    SetV3(locLightColor, (Vector3){ PLAYER_LIGHT_R, PLAYER_LIGHT_G, PLAYER_LIGHT_B });
    Render_UseWorldLight();
    Render_SetEmissive(false);
}

void Render_UseWorldLight(void)
{
    if (hasShader) SetF(locIsEntity, 0.0f);
}

void Render_UseEntityLight(Vector3 light)
{
    if (!hasShader) return;
    SetF(locIsEntity, 1.0f);
    SetV3(locEntityLight, light);
}

void Render_SetEmissive(bool on)
{
    if (hasShader) SetF(locEmissive, on ? 1.0f : 0.0f);
}

void Render_DrawWorld(const World *w, float time)
{
    int i;
    Render_UseWorldLight();
    for (i = 0; i < w->chunkCount; i++)
        DrawMesh(w->chunks[i], worldMat, MatrixIdentity());

    /* exit doors slide down into the floor as doorSlide goes 0 -> 1 */
    if (w->doorSlide < 1.0f) {
        for (i = 0; i < w->exitCount; i++) {
            Vector3 c = { w->exits[i].x + 0.5f, 1.5f, w->exits[i].z + 0.5f };
            Matrix m = MatrixMultiply(MatrixRotateY(w->exitYaw[i]),
                                      MatrixTranslate(c.x, -w->doorSlide * WALL_HEIGHT, c.z));
            Render_UseEntityLight(World_LightAt(w, c));
            DrawMesh(w->doorMesh, worldMat, m);
        }
        Render_UseWorldLight();
    }

    /* torch flames: small glowing cubes that flicker in size (raylib's default shader = unlit) */
    for (i = 0; i < w->torchCount; i++) {
        Vector3 p = w->torches[i].pos;
        float f = 1.0f + 0.15f * sinf(time * 13.0f + i * 1.7f) + 0.08f * sinf(time * 23.0f + i);
        DrawCube((Vector3){ p.x, p.y + 0.02f, p.z }, 0.15f * f, 0.2f * f, 0.15f * f, (Color){ 255, 120, 30, 255 });
        DrawCube((Vector3){ p.x, p.y + 0.0f, p.z }, 0.08f, 0.12f * f, 0.08f, (Color){ 255, 236, 150, 255 });
    }
}
