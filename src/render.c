/* render.c - world shader setup, per-frame uniforms and world drawing (see render.h). */
#include <stdio.h>
#include <math.h>
#include "render.h"
#include "textures.h"
#include "meshgen.h"
#include "raymath.h"
#include "rlgl.h"

#define SHADER_VS "assets/shaders/world.vs"
#define SHADER_FS "assets/shaders/world.fs"

static float    flare;        /* 0..1 torch flare when a bell tolls */
static bool     debugBright;  /* --bright: flat bright ambient to inspect geometry */
static bool     warm;         /* Sanctum: golden fog and ambient */
static Shader   shader;       /* module-private GPU resources */
static bool     hasShader;
static Material worldMat;     /* atlas material (props, characters) */
static Material mats[MAT_COUNT];  /* world materials */
static Mesh     leafWood, leafMetal;                         /* one exit door leaf */
static Mesh     chestWood, chestMetal, lidWood, lidMetal, chestGold;   /* reliquary chest */
static int locSnap, locAmbientTint, locFlashPos, locFlashColor, locFlashRadius;
static int locFogColor, locFogDensity, locAmbient, locFlicker, locLightPos, locLightRadius,
           locLightColor, locEntityLight, locIsEntity, locEmissive;

/* ------------------------------------------------------------ prop meshes */

/* Quad with UVs projected in the mesh's own space (props are small, so this is fine). */
static void PQuad(MeshBuilder *mb, Vector3 a, Vector3 b, Vector3 c, Vector3 d)
{
    Vector3 p[4] = { a, b, c, d };
    Vector3 n = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a));
    Vector2 uv[4];
    Color l[4];
    int k;
    if (Vector3Length(n) < 1e-6f) n = Vector3CrossProduct(Vector3Subtract(c, b), Vector3Subtract(a, b));
    n = Vector3Normalize(n);
    for (k = 0; k < 4; k++) { uv[k] = MB_WorldUV(Vector3Scale(p[k], 2.0f), n); l[k] = WHITE; }
    MB_Quad(mb, p, uv, l);
}

static void PBox(MeshBuilder *mb, Vector3 mn, Vector3 mx)
{
    PQuad(mb, (Vector3){ mn.x, mx.y, mx.z }, (Vector3){ mx.x, mx.y, mx.z }, (Vector3){ mx.x, mx.y, mn.z }, (Vector3){ mn.x, mx.y, mn.z });
    PQuad(mb, (Vector3){ mn.x, mn.y, mn.z }, (Vector3){ mx.x, mn.y, mn.z }, (Vector3){ mx.x, mn.y, mx.z }, (Vector3){ mn.x, mn.y, mx.z });
    PQuad(mb, (Vector3){ mx.x, mn.y, mx.z }, (Vector3){ mx.x, mn.y, mn.z }, (Vector3){ mx.x, mx.y, mn.z }, (Vector3){ mx.x, mx.y, mx.z });
    PQuad(mb, (Vector3){ mn.x, mn.y, mn.z }, (Vector3){ mn.x, mn.y, mx.z }, (Vector3){ mn.x, mx.y, mx.z }, (Vector3){ mn.x, mx.y, mn.z });
    PQuad(mb, (Vector3){ mn.x, mn.y, mx.z }, (Vector3){ mx.x, mn.y, mx.z }, (Vector3){ mx.x, mx.y, mx.z }, (Vector3){ mn.x, mx.y, mx.z });
    PQuad(mb, (Vector3){ mx.x, mn.y, mn.z }, (Vector3){ mn.x, mn.y, mn.z }, (Vector3){ mn.x, mx.y, mn.z }, (Vector3){ mx.x, mx.y, mn.z });
}

/* Pointed arch profile (same as the architecture): t 0..1 across the opening -> 0..1 height. */
static float ArchProfile(float t)
{
    const float r = 0.75f, apex = 0.70710678f;
    float d;
    if (t > 0.5f) t = 1.0f - t;
    d = t - r;
    return sqrtf(fmaxf(0.0f, r * r - d * d)) / apex;
}

/* Height of a door leaf's top edge at local x (0 = hinge, 1 = middle of the doorway), for the
 * usual 2-wide doorway under a 3.5 high corridor ceiling. */
static float LeafTop(float x)
{
    const float spring = CORRIDOR_HEIGHT - 0.70710678f * 2.0f;
    return spring + (CORRIDOR_HEIGHT - spring) * ArchProfile(x * 0.5f) - 0.02f;
}

/* One leaf of the tall pointed-arch double door: dark wood planks (local x 0..1 from the hinge,
 * thickness along z), plus iron straps and a ring handle on both sides. */
static void BuildDoorLeaf(void)
{
    const int seg = 8;
    const float t = 0.05f;
    MeshBuilder mb;
    int i, side;
    MB_Begin(&mb);
    for (i = 0; i < seg; i++) {
        float x0 = (float)i / seg, x1 = (float)(i + 1) / seg, y0 = LeafTop(x0), y1 = LeafTop(x1);
        PQuad(&mb, (Vector3){ x0, 0, t }, (Vector3){ x1, 0, t }, (Vector3){ x1, y1, t }, (Vector3){ x0, y0, t });     /* front */
        PQuad(&mb, (Vector3){ x1, 0, -t }, (Vector3){ x0, 0, -t }, (Vector3){ x0, y0, -t }, (Vector3){ x1, y1, -t }); /* back */
        PQuad(&mb, (Vector3){ x0, y0, t }, (Vector3){ x1, y1, t }, (Vector3){ x1, y1, -t }, (Vector3){ x0, y0, -t }); /* top edge */
    }
    PQuad(&mb, (Vector3){ 0, 0, -t }, (Vector3){ 0, 0, t }, (Vector3){ 0, LeafTop(0), t }, (Vector3){ 0, LeafTop(0), -t });  /* hinge edge */
    PQuad(&mb, (Vector3){ 1, 0, t }, (Vector3){ 1, 0, -t }, (Vector3){ 1, LeafTop(1), -t }, (Vector3){ 1, LeafTop(1), t });  /* middle edge */
    leafWood = MB_End(&mb);

    MB_Begin(&mb);
    for (side = -1; side <= 1; side += 2) {
        float z0 = side > 0 ? t : -t - 0.02f, z1 = side > 0 ? t + 0.02f : -t;
        PBox(&mb, (Vector3){ 0.02f, 0.45f, z0 }, (Vector3){ 0.96f, 0.56f, z1 });            /* straps */
        PBox(&mb, (Vector3){ 0.02f, 1.55f, z0 }, (Vector3){ 0.96f, 1.66f, z1 });
        PBox(&mb, (Vector3){ 0.02f, 0.45f, z0 }, (Vector3){ 0.10f, 1.66f, z1 });            /* hinge band */
        PBox(&mb, (Vector3){ 0.78f, 1.05f, z0 - side * 0.0f }, (Vector3){ 0.84f, 1.11f, z1 + side * 0.03f });  /* ring: mount */
        PBox(&mb, (Vector3){ 0.74f, 0.88f, z0 }, (Vector3){ 0.88f, 0.91f, z1 });            /* ring */
        PBox(&mb, (Vector3){ 0.74f, 0.88f, z0 }, (Vector3){ 0.77f, 1.05f, z1 });
        PBox(&mb, (Vector3){ 0.85f, 0.88f, z0 }, (Vector3){ 0.88f, 1.05f, z1 });
    }
    leafMetal = MB_End(&mb);
}

/* Low-poly reliquary chest: wooden body on four small iron feet, iron corner bands, a gold lock
 * plate; the lid (separate, hinged at its back edge) is a curved half-cylinder with iron bands. */
static void BuildChest(void)
{
    const int seg = 6;
    MeshBuilder mb;
    int i, k;

    MB_Begin(&mb);
    PBox(&mb, (Vector3){ -0.45f, 0.08f, -0.32f }, (Vector3){ 0.45f, 0.57f, 0.32f });
    chestWood = MB_End(&mb);

    MB_Begin(&mb);
    for (i = 0; i < 4; i++) {
        float sx = (i & 1) ? 1.0f : -1.0f, sz = (i & 2) ? 1.0f : -1.0f;
        PBox(&mb, (Vector3){ sx * 0.40f - 0.05f, 0.0f, sz * 0.27f - 0.05f }, (Vector3){ sx * 0.40f + 0.05f, 0.08f, sz * 0.27f + 0.05f });
        PBox(&mb, (Vector3){ sx * 0.45f - 0.04f, 0.08f, sz * 0.32f - 0.04f }, (Vector3){ sx * 0.45f + 0.04f, 0.58f, sz * 0.32f + 0.04f });
    }
    PBox(&mb, (Vector3){ -0.47f, 0.30f, -0.34f }, (Vector3){ 0.47f, 0.35f, 0.34f });          /* band around the middle */
    PBox(&mb, (Vector3){ -0.07f, 0.36f, 0.32f }, (Vector3){ 0.07f, 0.52f, 0.35f });          /* lock plate */
    chestMetal = MB_End(&mb);

    /* lid: hinge line at local z = 0, curving forward to z = 0.66 */
    MB_Begin(&mb);
    for (i = 0; i < seg; i++) {
        float a0 = PI * i / seg, a1 = PI * (i + 1) / seg;
        Vector3 p0 = { 0, 0.20f * sinf(a0), 0.33f - 0.33f * cosf(a0) }, p1 = { 0, 0.20f * sinf(a1), 0.33f - 0.33f * cosf(a1) };
        PQuad(&mb, (Vector3){ 0.46f, p0.y, p0.z }, (Vector3){ -0.46f, p0.y, p0.z }, (Vector3){ -0.46f, p1.y, p1.z }, (Vector3){ 0.46f, p1.y, p1.z });
        PQuad(&mb, (Vector3){ -0.46f, 0, 0.33f }, (Vector3){ -0.46f, p0.y, p0.z }, (Vector3){ -0.46f, p1.y, p1.z }, (Vector3){ -0.46f, p1.y, p1.z });  /* end caps */
        PQuad(&mb, (Vector3){ 0.46f, 0, 0.33f }, (Vector3){ 0.46f, p1.y, p1.z }, (Vector3){ 0.46f, p0.y, p0.z }, (Vector3){ 0.46f, p0.y, p0.z });
    }
    PQuad(&mb, (Vector3){ -0.46f, 0, 0 }, (Vector3){ 0.46f, 0, 0 }, (Vector3){ 0.46f, 0, 0.66f }, (Vector3){ -0.46f, 0, 0.66f });   /* underside */
    lidWood = MB_End(&mb);

    MB_Begin(&mb);
    for (k = -1; k <= 1; k += 2) {
        float x0 = k * 0.30f - 0.04f, x1 = k * 0.30f + 0.04f;
        for (i = 0; i < seg; i++) {
            float a0 = PI * i / seg, a1 = PI * (i + 1) / seg;
            Vector3 p0 = { 0, 0.215f * sinf(a0), 0.33f - 0.345f * cosf(a0) }, p1 = { 0, 0.215f * sinf(a1), 0.33f - 0.345f * cosf(a1) };
            PQuad(&mb, (Vector3){ x1, p0.y, p0.z }, (Vector3){ x0, p0.y, p0.z }, (Vector3){ x0, p1.y, p1.z }, (Vector3){ x1, p1.y, p1.z });
        }
    }
    lidMetal = MB_End(&mb);

    MB_Begin(&mb);            /* glow inside an open chest */
    MB_Box(&mb, (Vector3){ -0.36f, 0.40f, -0.24f }, (Vector3){ 0.36f, 0.56f, 0.24f }, TILE_FLAME, WHITE);
    chestGold = MB_End(&mb);
}

static void BuildProps(void)
{
    BuildDoorLeaf();
    BuildChest();
}

/* A matrix whose local axes are X, up, Z and whose origin is t. */
static Matrix Basis(Vector3 X, Vector3 Z, Vector3 t)
{
    return (Matrix){ X.x, 0.0f, Z.x, t.x,
                     X.y, 1.0f, Z.y, t.y,
                     X.z, 0.0f, Z.z, t.z,
                     0.0f, 0.0f, 0.0f, 1.0f };
}

/* Both leaves of an exit door. The left leaf hinges at a, the right one at b; opening swings
 * them toward the inside of the wing. */
static void DrawDoorway(const World *w, const Doorway *d)
{
    Vector3 t = Vector3Subtract(d->b, d->a), mid;
    float width = Vector3Length(t), angle = 1.45f * w->doorSlide, sy = d->top / CORRIDOR_HEIGHT;
    Matrix left, right;
    t = Vector3Scale(t, 1.0f / width);
    mid = Vector3Scale(Vector3Add(d->a, d->b), 0.5f);
    left = MatrixMultiply(MatrixMultiply(MatrixScale(width * 0.5f, sy, 1.0f), MatrixRotateY(-angle)), Basis(t, d->inward, d->a));
    right = MatrixMultiply(MatrixMultiply(MatrixScale(width * 0.5f, sy, 1.0f), MatrixRotateY(angle)),
                           Basis(Vector3Negate(t), Vector3Negate(d->inward), d->b));
    Render_UseEntityLight(World_LightAt(w, (Vector3){ mid.x + d->inward.x, 1.5f, mid.z + d->inward.z }));
    DrawMesh(leafWood, mats[MAT_WOOD], left);
    DrawMesh(leafMetal, mats[MAT_METAL], left);
    DrawMesh(leafWood, mats[MAT_WOOD], right);
    DrawMesh(leafMetal, mats[MAT_METAL], right);
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
    {
        int i;
        for (i = 0; i < MAT_COUNT; i++) {
            mats[i] = LoadMaterialDefault();
            mats[i].maps[MATERIAL_MAP_DIFFUSE].texture = Textures_Material(i);
            if (hasShader) mats[i].shader = shader;
        }
    }
    BuildProps();
}

void Render_Shutdown(void)
{
    UnloadMesh(leafWood);
    UnloadMesh(leafMetal);
    UnloadMesh(chestWood);
    UnloadMesh(chestMetal);
    UnloadMesh(lidWood);
    UnloadMesh(lidMetal);
    UnloadMesh(chestGold);
    worldMat.maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ 0 };   /* owned by textures.c */
    worldMat.shader = (Shader){ 0 };                                  /* unloaded below */
    UnloadMaterial(worldMat);
    {
        int i;
        for (i = 0; i < MAT_COUNT; i++) {
            mats[i].maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ 0 };   /* owned by textures.c */
            mats[i].shader = (Shader){ 0 };
            UnloadMaterial(mats[i]);
        }
    }
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
    SetV3(locFogColor, warm ? (Vector3){ SANCTUM_FOG_R, SANCTUM_FOG_G, SANCTUM_FOG_B }
                            : (Vector3){ FOG_COLOR_R, FOG_COLOR_G, FOG_COLOR_B });
    SetF(locFogDensity, wing->fogDensity);
    SetF(locAmbient, debugBright ? 1.6f : wing->ambient);
    SetV3(locAmbientTint, warm ? (Vector3){ SANCTUM_TINT_R, SANCTUM_TINT_G, SANCTUM_TINT_B }
                               : (Vector3){ AMBIENT_TINT_R, AMBIENT_TINT_G, AMBIENT_TINT_B });
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

void Render_SetWarm(bool on)
{
    warm = on;
}

void Render_DrawWorld(const World *w, float time)
{
    int i;
    Render_UseWorldLight();
    for (i = 0; i < w->partCount; i++) {
        int m = w->parts[i].mat;
        bool glow = m == MAT_GLASS || m == MAT_FLAME;
        if (m == MAT_COBWEB) continue;                       /* drawn in the transparent pass */
        if (glow) Render_SetEmissive(true);
        DrawMesh(w->parts[i].mesh, mats[m], MatrixIdentity());
        if (glow) Render_SetEmissive(false);
    }

    /* exit doors: two leaves per doorway swing inward as doorSlide goes 0 -> 1 */
    for (i = 0; i < w->doorwayCount; i++) DrawDoorway(w, &w->doorways[i]);
    Render_UseWorldLight();

    /* torch flames: small glowing cubes that flicker in size (raylib's default shader = unlit) */
    for (i = 0; i < w->torchCount; i++) {
        Vector3 p = w->torches[i].pos;
        float f = (1.0f + 0.15f * sinf(time * 13.0f + i * 1.7f) + 0.08f * sinf(time * 23.0f + i)) * (1.0f + 0.9f * flare);
        DrawCube((Vector3){ p.x, p.y + 0.02f, p.z }, 0.15f * f, 0.2f * f, 0.15f * f, (Color){ 255, 120, 30, 255 });
        DrawCube((Vector3){ p.x, p.y + 0.0f, p.z }, 0.08f, 0.12f * f, 0.08f, (Color){ 255, 236, 150, 255 });
    }
}

/* Transparent world parts (cobwebs): after all opaque geometry, without depth writes. */
void Render_DrawWorldTransparent(const World *w)
{
    int i;
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    Render_UseWorldLight();
    for (i = 0; i < w->partCount; i++)
        if (w->parts[i].mat == MAT_COBWEB) DrawMesh(w->parts[i].mesh, mats[MAT_COBWEB], MatrixIdentity());
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

/* Material of a world material id (for props drawn outside the static meshes). */
Material Render_Material(int mat)
{
    return mats[mat >= 0 && mat < MAT_COUNT ? mat : 0];
}

void Render_DrawChest(Vector3 pos, float yaw, float lid, Vector3 light)
{
    Matrix base = MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(pos.x, pos.y, pos.z));
    /* lid swings up around the back edge (local z = -0.33, top of the body) */
    Matrix lidM = MatrixMultiply(MatrixMultiply(MatrixRotateX(-1.9f * lid), MatrixTranslate(0.0f, 0.57f, -0.33f)), base);
    Render_UseEntityLight(light);
    DrawMesh(chestWood, mats[MAT_WOOD], base);
    DrawMesh(chestMetal, mats[MAT_METAL], base);
    DrawMesh(lidWood, mats[MAT_WOOD], lidM);
    DrawMesh(lidMetal, mats[MAT_METAL], lidM);
    if (lid > 0.05f) {
        Render_SetEmissive(true);
        DrawMesh(chestGold, worldMat, base);
        Render_SetEmissive(false);
    }
    Render_UseWorldLight();
}

void Render_SetDebugBright(bool on)
{
    debugBright = on;
}
