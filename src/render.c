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
static float    lightning;    /* 0..1 lightning flash */
static Shader   shader;       /* module-private GPU resources */
static bool     hasShader;
static Material worldMat;     /* atlas material (props, characters) */
static Material mats[MAT_COUNT];  /* world materials */
static Mesh     leafWood, leafMetal;                         /* one exit door leaf */
static Mesh     chestWood, chestMetal, lidWood, lidMetal, chestGold;   /* reliquary chest */
static int locSnap, locAmbientTint, locFlashPos, locFlashColor, locFlashRadius;
static int locFogColor, locFogDensity, locAmbient, locLightPos, locLightRadius, locLightColor,
           locExtraAmbient, locEmissive, locEmissiveBoost, locSpecular,
           locLightCount, locLightsPos, locLightsColor, locLightsRadius;
static Camera3D frameCam;     /* camera of the frame being drawn (flame billboards) */

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
        locLightPos = GetShaderLocation(shader, "lightPos");
        locLightRadius = GetShaderLocation(shader, "lightRadius");
        locLightColor = GetShaderLocation(shader, "lightColor");
        locExtraAmbient = GetShaderLocation(shader, "extraAmbient");
        locSpecular = GetShaderLocation(shader, "specular");
        locLightCount = GetShaderLocation(shader, "lightCount");
        locLightsPos = GetShaderLocation(shader, "lightsPos");
        locLightsColor = GetShaderLocation(shader, "lightsColor");
        locLightsRadius = GetShaderLocation(shader, "lightsRadius");
        locEmissive = GetShaderLocation(shader, "emissive");
        locEmissiveBoost = GetShaderLocation(shader, "emissiveBoost");
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

/* ---- light selection: the world has up to a few hundred lights, the shader takes the 16 nearest
 * visible ones. Each shader slot fades its light in and out so lights never pop. */

typedef struct { int light; float weight; } LightSlot;

static LightSlot slots[MAX_LIGHTS];
static Vector3   dynPos[MAX_DYN_LIGHTS], dynColor[MAX_DYN_LIGHTS];
static float     dynRadius[MAX_DYN_LIGHTS];
static int       dynCount;
static bool      snapLights = true;       /* next frame: show the chosen lights at once */
static float     lastTime;
static Vector3   lastCam;

void Render_ResetLights(void)
{
    int i;
    for (i = 0; i < MAX_LIGHTS; i++) slots[i] = (LightSlot){ -1, 0.0f };
    snapLights = true;
}

void Render_AddDynamicLight(Vector3 pos, Vector3 color, float radius)
{
    if (dynCount >= MAX_DYN_LIGHTS) return;
    dynPos[dynCount] = pos;
    dynColor[dynCount] = color;
    dynRadius[dynCount] = radius;
    dynCount++;
}

/* Candle flicker for light i (each light has its own rhythm). */
static float LightFlicker(int i, float time)
{
    float p = i * 1.37f;
    return 0.86f + 0.08f * sinf(time * 7.3f + p) + 0.04f * sinf(time * 13.1f + p * 2.0f) + 0.02f * sinf(time * 23.7f + p);
}

/* The (up to) MAX_LIGHTS nearest lights the camera or the player can see (or that are very
 * close), nearest first. */
static int ChooseLights(const World *w, Vector3 cam, Vector3 player, int *out)
{
    float bestD[MAX_LIGHTS];
    int n = 0, i, k;
    Vector3 head = { player.x, 1.5f, player.z };
    for (i = 0; i < w->torchCount; i++) {
        const Torch *t = &w->torches[i];
        float d = Vector3Distance(cam, t->pos);
        if (d > LIGHT_VIEW_RANGE + t->radius * 0.5f) continue;
        if (n == MAX_LIGHTS && d >= bestD[n - 1]) continue;
        if (d > 3.0f && !World_LineOfSight(w, cam, t->pos) && !World_LineOfSight(w, head, t->pos)) continue;
        /* insert sorted */
        k = n < MAX_LIGHTS ? n++ : MAX_LIGHTS - 1;
        while (k > 0 && bestD[k - 1] > d) { bestD[k] = bestD[k - 1]; out[k] = out[k - 1]; k--; }
        bestD[k] = d;
        out[k] = i;
    }
    return n;
}

static void UpdateSlots(const World *w, Vector3 cam, Vector3 player, float dt)
{
    int want[MAX_LIGHTS], n = ChooseLights(w, cam, player, want), i, s;
    bool placed[MAX_LIGHTS] = { false };
    for (s = 0; s < MAX_LIGHTS; s++) {
        bool keep = false;
        if (slots[s].light < 0) continue;
        for (i = 0; i < n; i++) if (want[i] == slots[s].light) { keep = true; placed[i] = true; }
        slots[s].weight += (keep ? 1.0f : -1.0f) * LIGHT_FADE_SPEED * dt;
        if (snapLights) slots[s].weight = keep ? 1.0f : 0.0f;
        if (slots[s].weight >= 1.0f) slots[s].weight = 1.0f;
        if (slots[s].weight <= 0.0f) slots[s] = (LightSlot){ -1, 0.0f };
    }
    for (i = 0; i < n; i++) {
        if (placed[i]) continue;
        for (s = 0; s < MAX_LIGHTS; s++) {
            if (slots[s].light >= 0) continue;
            slots[s] = (LightSlot){ want[i], snapLights ? 1.0f : 0.0f };
            break;
        }
    }
    snapLights = false;
}

void Render_BeginFrame(const WingConfig *wing, const World *w, Camera3D cam, Vector3 playerPos, float time)
{
    Vector3 pos[MAX_LIGHTS + MAX_DYN_LIGHTS], col[MAX_LIGHTS + MAX_DYN_LIGHTS];
    float rad[MAX_LIGHTS + MAX_DYN_LIGHTS], dt = Clamp(time - lastTime, 0.0f, 0.1f);
    int n = 0, i;

    frameCam = cam;
    if (Vector3Distance(cam.position, lastCam) > 4.0f) snapLights = true;    /* teleported (autotest, respawn) */
    lastCam = cam.position;
    lastTime = time;
    if (w) UpdateSlots(w, cam.position, playerPos, dt);
    if (!hasShader) { dynCount = 0; return; }

    /* moving lights first, then the faded world lights */
    for (i = 0; i < dynCount; i++, n++) { pos[n] = dynPos[i]; col[n] = dynColor[i]; rad[n] = dynRadius[i]; }
    dynCount = 0;
    for (i = 0; i < MAX_LIGHTS && w && n < MAX_LIGHTS + MAX_DYN_LIGHTS; i++) {
        const Torch *t;
        float k;
        if (slots[i].light < 0 || slots[i].light >= w->torchCount) continue;
        t = &w->torches[slots[i].light];
        if (t->flicker > 0.0f) {
            k = LightFlicker(slots[i].light, time) * wing->lightMul * FLAME_LIGHT_SCALE * (1.0f + 0.7f * flare);
            col[n] = Vector3Scale(t->color, k * slots[i].weight);
        } else {
            /* moonlit window: lightning floods it with white-blue light */
            Vector3 c = Vector3Lerp(t->color, (Vector3){ 0.9f, 0.95f, 1.1f }, lightning);
            col[n] = Vector3Scale(c, (1.0f + 4.0f * lightning) * slots[i].weight);
        }
        pos[n] = t->pos;
        rad[n] = t->radius;
        n++;
    }
    SetShaderValue(shader, locLightCount, &n, SHADER_UNIFORM_INT);
    if (n > 0) {
        SetShaderValueV(shader, locLightsPos, pos, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(shader, locLightsColor, col, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(shader, locLightsRadius, rad, SHADER_UNIFORM_FLOAT, n);
    }

    SetV3(shader.locs[SHADER_LOC_VECTOR_VIEW], cam.position);
    SetV3(locFogColor, (Vector3){ wing->fog[0], wing->fog[1], wing->fog[2] });
    SetF(locFogDensity, wing->fogDensity);
    SetF(locAmbient, debugBright ? 1.6f : wing->ambient + lightning * 0.35f);
    SetF(locEmissiveBoost, 1.0f + 2.5f * lightning);
    SetV3(locAmbientTint, (Vector3){ wing->tint[0], wing->tint[1], wing->tint[2] });
    {
        Vector2 snap = { PS1_WOBBLE ? PS1_WOBBLE_GRID_W : 0.0f, PS1_WOBBLE_GRID_H };
        SetShaderValue(shader, locSnap, &snap, SHADER_UNIFORM_VEC2);
    }
    /* the player's soft light hangs between Kael and the camera, a little above */
    {
        Vector3 head = { playerPos.x, playerPos.y + 1.5f, playerPos.z };
        Vector3 lp = Vector3Lerp(head, cam.position, 0.4f);
        lp.y = playerPos.y + PLAYER_LIGHT_HEIGHT;
        SetV3(locLightPos, lp);
    }
    SetF(locLightRadius, wing->playerLightRadius);
    SetV3(locLightColor, (Vector3){ PLAYER_LIGHT_R, PLAYER_LIGHT_G, PLAYER_LIGHT_B });
    SetF(locSpecular, 0.0f);
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
    if (hasShader) SetF(locExtraAmbient, 0.0f);
}

/* Characters and moving props are lit per pixel like the world; `light` (the CPU estimate of the
 * light where they stand) only adds a little extra ambient so silhouettes stay readable. */
void Render_UseEntityLight(Vector3 light)
{
    float avg = (light.x + light.y + light.z) / 3.0f;
    if (hasShader) SetF(locExtraAmbient, CHAR_AMBIENT + 0.12f * fminf(avg, 1.0f));
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
    for (i = 0; i < w->partCount; i++) {
        int m = w->parts[i].mat;
        bool glow = m == MAT_GLASS || m == MAT_FLAME || m == MAT_POTION;
        if (m == MAT_COBWEB) continue;                       /* drawn in the transparent pass */
        if (glow) Render_SetEmissive(true);
        if (m == MAT_FLOOR && hasShader) SetF(locSpecular, FLOOR_SPECULAR);
        DrawMesh(w->parts[i].mesh, mats[m], MatrixIdentity());
        if (m == MAT_FLOOR && hasShader) SetF(locSpecular, 0.0f);
        if (glow) Render_SetEmissive(false);
    }

    /* exit doors: two leaves per doorway swing inward as doorSlide goes 0 -> 1 */
    for (i = 0; i < w->doorwayCount; i++) DrawDoorway(w, &w->doorways[i]);
    Render_UseWorldLight();

    (void)time;
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

/* Flames: camera-facing flame sprites with a soft glow around them, drawn additively so they
 * shine through the fog. They flicker in size and flare when a bell tolls. */
void Render_DrawFlames(const World *w, float time)
{
    Texture2D tex = Textures_Material(MAT_FLAME);
    int i;
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    for (i = 0; i < w->flameCount; i++) {
        Vector3 p = w->flames[i].pos;
        float k = w->flames[i].size;
        float f = (1.0f + 0.15f * sinf(time * 13.0f + i * 1.7f) + 0.08f * sinf(time * 23.0f + i)) * (1.0f + 0.6f * flare);
        DrawBillboard(frameCam, tex, (Vector3){ p.x, p.y + 0.06f * k, p.z }, 0.26f * k * f, (Color){ 255, 220, 170, 255 });
        DrawBillboard(frameCam, tex, (Vector3){ p.x, p.y + 0.02f * k, p.z }, 0.75f * k * f, (Color){ 255, 120, 40, 46 });
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
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

void Render_SetLightning(float amount)
{
    lightning = amount;
}

/* One shaft face from window edge (a, b) down to floor edge (c, d), fading out toward the floor. */
static void ShaftQuad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color top, Color bottom)
{
    rlColor4ub(top.r, top.g, top.b, top.a);    rlVertex3f(a.x, a.y, a.z);
    rlColor4ub(bottom.r, bottom.g, bottom.b, bottom.a); rlVertex3f(d.x, d.y, d.z);
    rlColor4ub(bottom.r, bottom.g, bottom.b, bottom.a); rlVertex3f(c.x, c.y, c.z);
    rlColor4ub(top.r, top.g, top.b, top.a);    rlVertex3f(b.x, b.y, b.z);
}

/* Moonlight falling through each window onto the floor: a translucent pale-blue prism drawn
 * additively (both sides, no depth writes), a soft patch on the floor and a few dust motes
 * drifting slowly inside it. Lightning makes them flare white. */
void Render_DrawWindowShafts(const World *w, float time)
{
    const float fall = 0.7f;                         /* sideways drift of the light per unit of drop */
    float k = 1.0f + 3.0f * lightning;
    Color top = { (unsigned char)fminf(255.0f, 120 + 100 * lightning), (unsigned char)fminf(255.0f, 150 + 80 * lightning), 255,
                  (unsigned char)fminf(255.0f, 34 * k) };
    Color bottom = { top.r, top.g, top.b, (unsigned char)fminf(255.0f, 5 * k) };
    Color floorCol = { top.r, top.g, top.b, (unsigned char)fminf(255.0f, 22 * k) };
    int i, m;

    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_QUADS);
    for (i = 0; i < w->windowCount; i++) {
        const Window *wn = &w->windows[i];
        Vector3 s = Vector3Scale(wn->side, wn->halfWidth), n = wn->normal;
        Vector3 c0 = { wn->center.x, wn->sill, wn->center.z }, c1 = { wn->center.x, wn->top - 0.25f, wn->center.z };
        Vector3 wl0 = Vector3Subtract(c0, s), wr0 = Vector3Add(c0, s), wl1 = Vector3Subtract(c1, s), wr1 = Vector3Add(c1, s);
        /* where each window edge lands on the floor */
        Vector3 fl0 = Vector3Add(wl0, (Vector3){ n.x * wn->sill * fall, -wn->sill + 0.02f, n.z * wn->sill * fall });
        Vector3 fr0 = Vector3Add(wr0, (Vector3){ n.x * wn->sill * fall, -wn->sill + 0.02f, n.z * wn->sill * fall });
        Vector3 fl1 = Vector3Add(wl1, (Vector3){ n.x * c1.y * fall, -c1.y + 0.02f, n.z * c1.y * fall });
        Vector3 fr1 = Vector3Add(wr1, (Vector3){ n.x * c1.y * fall, -c1.y + 0.02f, n.z * c1.y * fall });
        ShaftQuad(wl1, wr1, fr1, fl1, top, bottom);            /* upper sheet */
        ShaftQuad(wl0, wr0, fr0, fl0, top, bottom);            /* lower sheet */
        ShaftQuad(wl1, wl0, fl0, fl1, top, bottom);            /* sides */
        ShaftQuad(wr0, wr1, fr1, fr0, top, bottom);
        ShaftQuad(fl0, fr0, fr1, fl1, floorCol, floorCol);     /* patch on the floor */
    }
    rlEnd();

    /* dust motes: tiny specks drifting down and sideways through each shaft */
    for (i = 0; i < w->windowCount; i++) {
        const Window *wn = &w->windows[i];
        for (m = 0; m < DUST_PER_WINDOW; m++) {
            float t = fmodf(time * 0.04f + m * 0.173f + i * 0.31f, 1.0f);
            float h = wn->top - 0.4f - t * (wn->top - 0.6f);
            float side = sinf(time * 0.3f + m * 2.1f + i) * wn->halfWidth * 0.9f;
            float drop = wn->top - 0.4f - h;
            Vector3 p = Vector3Add(wn->center, Vector3Add(Vector3Scale(wn->side, side),
                                   (Vector3){ wn->normal.x * (0.25f + drop * fall), h - wn->center.y, wn->normal.z * (0.25f + drop * fall) }));
            unsigned char a = (unsigned char)(140 * sinf(t * PI));
            DrawCube(p, 0.025f, 0.025f, 0.025f, (Color){ 200, 215, 255, a });
        }
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
}

void Render_SetDebugBright(bool on)
{
    debugBright = on;
}
