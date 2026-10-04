/* render.c - world shader setup, per-frame uniforms and world drawing (see render.h). */
#include <stdio.h>
#include <math.h>
#include "render.h"
#include "textures.h"
#include "meshgen.h"
#include "raymath.h"

#define SHADER_VS "assets/shaders/world.vs"
#define SHADER_FS "assets/shaders/world.fs"

static float    flare;        /* 0..1 torch flare when a bell tolls */
static Shader   shader;       /* module-private GPU resources */
static bool     hasShader;
static Material worldMat;
static Mesh     doorMesh, chestBody, chestLid, chestGold;   /* prop meshes */
static int locSnap, locAmbientTint, locFlashPos, locFlashColor, locFlashRadius;
static int locFogColor, locFogDensity, locAmbient, locFlicker, locLightPos, locLightRadius,
           locLightColor, locEntityLight, locIsEntity, locEmissive;

/* Exit door (4 stacked iron blocks, thin along local Z), chest body + lid (hinged at the back). */
static void BuildProps(void)
{
    MeshBuilder mb;
    int k;
    MB_Begin(&mb);
    for (k = 0; k < (int)WALL_HEIGHT; k++)
        MB_Box(&mb, (Vector3){ -0.49f, (float)k, -0.12f }, (Vector3){ 0.49f, k + 1.0f, 0.12f }, TILE_IRON_DOOR, WHITE);
    doorMesh = MB_End(&mb);

    MB_Begin(&mb);
    MB_Box(&mb, (Vector3){ -0.45f, 0.0f, -0.32f }, (Vector3){ 0.45f, 0.55f, 0.32f }, TILE_CHEST, WHITE);
    MB_Box(&mb, (Vector3){ -0.47f, 0.0f, -0.34f }, (Vector3){ -0.39f, 0.57f, 0.34f }, TILE_IRON, WHITE);   /* iron corners */
    MB_Box(&mb, (Vector3){ 0.39f, 0.0f, -0.34f }, (Vector3){ 0.47f, 0.57f, 0.34f }, TILE_IRON, WHITE);
    chestBody = MB_End(&mb);

    MB_Begin(&mb);            /* lid: hinge at the origin, extends forward (+Z) */
    MB_Box(&mb, (Vector3){ -0.46f, 0.0f, 0.0f }, (Vector3){ 0.46f, 0.24f, 0.66f }, TILE_CHEST, WHITE);
    chestLid = MB_End(&mb);

    MB_Begin(&mb);            /* treasure inside an open chest */
    MB_Box(&mb, (Vector3){ -0.36f, 0.40f, -0.24f }, (Vector3){ 0.36f, 0.56f, 0.24f }, TILE_FLAME, WHITE);
    chestGold = MB_End(&mb);
}

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
        locSnap = GetShaderLocation(shader, "snapGrid");
        locFlashPos = GetShaderLocation(shader, "flashPos");
        locFlashColor = GetShaderLocation(shader, "flashColor");
        locFlashRadius = GetShaderLocation(shader, "flashRadius");
        locAmbientTint = GetShaderLocation(shader, "ambientTint");
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
    BuildProps();
}

void Render_Shutdown(void)
{
    UnloadMesh(doorMesh);
    UnloadMesh(chestBody);
    UnloadMesh(chestLid);
    UnloadMesh(chestGold);
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
    SetV3(locAmbientTint, (Vector3){ AMBIENT_TINT_R, AMBIENT_TINT_G, AMBIENT_TINT_B });
    {
        Vector2 snap = { PS1_WOBBLE ? PS1_WOBBLE_GRID_W : 0.0f, PS1_WOBBLE_GRID_H };
        SetShaderValue(shader, locSnap, &snap, SHADER_UNIFORM_VEC2);
    }
    SetF(locFlicker, Render_Flicker(time) * (1.0f + 0.7f * flare));
    SetV3(locLightPos, (Vector3){ playerPos.x, playerPos.y + PLAYER_LIGHT_HEIGHT, playerPos.z });
    SetF(locLightRadius, wing->playerLightRadius);
    SetV3(locLightColor, (Vector3){ PLAYER_LIGHT_R, PLAYER_LIGHT_G, PLAYER_LIGHT_B });
    Render_SetFlash((Vector3){ 0 }, (Vector3){ 0 }, 1.0f);
    Render_UseWorldLight();
    Render_SetEmissive(false);
}

void Render_SetFlash(Vector3 pos, Vector3 color, float radius)
{
    if (!hasShader) return;
    SetV3(locFlashPos, pos);
    SetV3(locFlashColor, color);
    SetF(locFlashRadius, radius);
}

void Render_UseWorldLight(void)
{
    if (hasShader) SetF(locIsEntity, 0.0f);
}

void Render_UseEntityLight(Vector3 light)
{
    /* half-desaturate torch light on characters so steel and bone don't turn orange-pink */
    float grey = (light.x + light.y + light.z) / 3.0f;
    if (!hasShader) return;
    light = Vector3Lerp(light, (Vector3){ grey, grey, grey }, 0.45f);
    SetF(locIsEntity, 1.0f);
    SetV3(locEntityLight, light);
}

void Render_SetEmissive(bool on)
{
    Render_SetEmissiveMode(on ? 1 : 0);
}

void Render_SetEmissiveMode(int mode)
{
    if (hasShader) SetF(locEmissive, (float)mode);
}

void Render_SetFlare(float amount)
{
    flare = amount;
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
            DrawMesh(doorMesh, worldMat, m);
        }
        Render_UseWorldLight();
    }

    /* torch flames: small glowing cubes that flicker in size (raylib's default shader = unlit) */
    for (i = 0; i < w->torchCount; i++) {
        Vector3 p = w->torches[i].pos;
        float f = (1.0f + 0.15f * sinf(time * 13.0f + i * 1.7f) + 0.08f * sinf(time * 23.0f + i)) * (1.0f + 0.9f * flare);
        DrawCube((Vector3){ p.x, p.y + 0.02f, p.z }, 0.15f * f, 0.2f * f, 0.15f * f, (Color){ 255, 120, 30, 255 });
        DrawCube((Vector3){ p.x, p.y + 0.0f, p.z }, 0.08f, 0.12f * f, 0.08f, (Color){ 255, 236, 150, 255 });
    }
}

void Render_DrawChest(Vector3 pos, float yaw, float lid, Vector3 light)
{
    Matrix base = MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(pos.x, pos.y, pos.z));
    /* lid swings up around the back edge (local z = -0.33, top of the body) */
    Matrix lidM = MatrixMultiply(MatrixMultiply(MatrixRotateX(-1.9f * lid), MatrixTranslate(0.0f, 0.55f, -0.33f)), base);
    Render_UseEntityLight(light);
    DrawMesh(chestBody, worldMat, base);
    DrawMesh(chestLid, worldMat, lidM);
    if (lid > 0.05f) {
        Render_SetEmissive(true);
        DrawMesh(chestGold, worldMat, base);
        Render_SetEmissive(false);
    }
    Render_UseWorldLight();
}
