/* relics.c - Soul Relics, the serpent's cage, the Ember Serpent and the Abbot's shield (relics.h).
 *
 * The relic moment: the last chest of a wing bursts open and its Soul Relic rises, floating and
 * slowly turning, glowing red with a low whisper while the nearby lights dim. Three hits of Red
 * Lightning crack it; the third shatters it, the wing's cursed bell breaks and the exit opens.
 *
 * The Bell Tower: a raised dais with a domed iron cage holds the coiled Ember Serpent. The cage is
 * sealed (glowing runes, spells spark off) until all five relics are destroyed; then 6 hits break
 * it and the serpent fights: it circles the player at a distance and every 2.5 s rears up glowing
 * red and lunges 4 units forward. While it lives, the Red Abbot waits behind a red shield. */
#include <math.h>
#include <string.h>
#include "relics.h"
#include "game.h"
#include "geo.h"
#include "render.h"
#include "audio.h"
#include "character.h"
#include "textures.h"
#include "raymath.h"
#include "rlgl.h"

const char *const RELIC_NAMES[WING_COUNT] = RELIC_NAME_TABLE;

#define RELIC_PARTS 8

typedef struct { Mesh mesh[RELIC_PARTS]; int mat[RELIC_PARTS]; int count; } PartSet;

static PartSet relicMesh[WING_COUNT];     /* module-private GPU resources */
static PartSet cageDais;
static Mesh    unitCyl, unitSphere, emberSphere, crackBox;
static int     relicSlot[WING_COUNT];
static bool    slotsAsked;

/* ============================================================ mesh building */

static float Hash01(int x, int y, int seed)
{
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ (unsigned int)seed * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
}

static void Flush(Geo *g, PartSet *ps)
{
    int m;
    ps->count = 0;
    for (m = 0; m < MAT_COUNT; m++) {
        Mesh mesh = MB_End(&g->mb[m]);
        if (mesh.vertexCount == 0) continue;
        if (ps->count >= RELIC_PARTS) { UnloadMesh(mesh); continue; }
        ps->mesh[ps->count] = mesh;
        ps->mat[ps->count] = m;
        ps->count++;
    }
}

static void Begin(Geo *g)
{
    int m;
    memset(g, 0, sizeof(*g));
    for (m = 0; m < MAT_COUNT; m++) MB_Begin(&g->mb[m]);
}

static void BoxC(Geo *g, int mat, float cx, float cy, float cz, float hx, float hy, float hz)
{
    Geo_Box(g, mat, (Vector3){ cx - hx, cy - hy, cz - hz }, (Vector3){ cx + hx, cy + hy, cz + hz });
}

/* A faceted gem (two cones base to base) around the vertical axis at c. */
static void Gem(Geo *g, int mat, Vector3 c, float r, float h)
{
    const float rr[3] = { 0.0f, r, 0.0f }, yy[3] = { -h * 0.6f, 0.0f, h * 0.4f };
    Geo_Lathe(g, mat, c, rr, yy, 3, 6, false);
}

/* 1 - The Ashbound Grimoire: black leather, iron corner caps, iron clasp, red wax seal. */
static void BuildGrimoire(Geo *g)
{
    const float sr[2] = { 0.05f, 0.05f }, sy[2] = { 0.0f, 0.02f };
    int i;
    BoxC(g, MAT_LEATHER, 0.0f, 0.058f, 0.0f, 0.25f, 0.013f, 0.19f);       /* covers */
    BoxC(g, MAT_LEATHER, 0.0f, -0.058f, 0.0f, 0.25f, 0.013f, 0.19f);
    BoxC(g, MAT_LEATHER, -0.235f, 0.0f, 0.0f, 0.02f, 0.07f, 0.19f);       /* spine */
    BoxC(g, MAT_BONE, 0.01f, 0.0f, 0.0f, 0.225f, 0.045f, 0.172f);        /* pages */
    for (i = 0; i < 4; i++) {
        float x = (i & 1) ? 0.22f : -0.22f, z = (i & 2) ? 0.16f : -0.16f;
        BoxC(g, MAT_METAL, x, 0.0f, z, 0.04f, 0.078f, 0.04f);            /* corner caps */
    }
    BoxC(g, MAT_METAL, 0.255f, 0.0f, 0.0f, 0.02f, 0.085f, 0.05f);        /* clasp */
    BoxC(g, MAT_METAL, 0.18f, 0.074f, 0.0f, 0.08f, 0.006f, 0.035f);
    Geo_Lathe(g, MAT_RUBY, (Vector3){ 0.05f, 0.071f, 0.0f }, sr, sy, 2, 8, true);   /* wax seal */
}

/* 2 - The Ember Ring: heavy dark-gold band (built lying flat, stood up when drawn), big red stone. */
static void BuildRing(Geo *g)
{
    float r[9], y[9];
    int k;
    for (k = 0; k < 9; k++) { float a = 2.0f * PI * k / 8.0f; r[k] = 0.17f + 0.045f * cosf(a); y[k] = 0.045f * sinf(a); }
    Geo_Lathe(g, MAT_GOLD, (Vector3){ 0 }, r, y, 9, 14, false);
    BoxC(g, MAT_GOLD, 0.0f, 0.0f, -0.215f, 0.07f, 0.05f, 0.03f);         /* setting */
    {
        /* the stone faces outward (it becomes the top of the standing ring) */
        const float rr[3] = { 0.0f, 0.085f, 0.0f }, yy[3] = { 0.0f, 0.05f, 0.11f };
        Geo_Lathe(g, MAT_RUBY, (Vector3){ 0.0f, -0.05f, -0.26f }, rr, yy, 3, 6, false);
    }
}

/* 3 - The Moonsilver Locket: oval silver case (built flat, stood up), crescent, blue seam, chain. */
static void BuildLocket(Geo *g)
{
    const float r[5] = { 0.0f, 0.13f, 0.16f, 0.13f, 0.0f }, y[5] = { -0.05f, -0.04f, 0.0f, 0.04f, 0.05f };
    const float sr[2] = { 0.163f, 0.163f }, sy[2] = { -0.007f, 0.007f };
    int i;
    Geo_Lathe(g, MAT_SILVER, (Vector3){ 0 }, r, y, 5, 12, false);
    Geo_Lathe(g, MAT_GLASS, (Vector3){ 0 }, sr, sy, 2, 12, false);                       /* glowing seam */
    for (i = 0; i < 5; i++) {                                                            /* engraved crescent */
        float a = -1.1f + i * 0.55f;
        BoxC(g, MAT_METAL, cosf(a) * 0.07f - 0.01f, 0.05f, sinf(a) * 0.07f, 0.014f, 0.004f, 0.014f);
    }
    for (i = 0; i < 4; i++) BoxC(g, MAT_SILVER, 0.0f, 0.0f, -0.18f - i * 0.045f, 0.01f, 0.01f, 0.02f);   /* chain */
}

/* 4 - The Chalice of Cinders: dark gold, hexagonal foot, knotted stem, glowing embers inside. */
static void BuildChalice(Geo *g)
{
    const float r[9] = { 0.15f, 0.15f, 0.04f, 0.03f, 0.055f, 0.03f, 0.05f, 0.15f, 0.165f };
    const float y[9] = { -0.26f, -0.23f, -0.2f, -0.12f, -0.09f, -0.05f, 0.0f, 0.16f, 0.2f };
    const float er[1] = { 0.14f }, ey[1] = { 0.15f };
    Geo_Lathe(g, MAT_GOLD, (Vector3){ 0 }, r, y, 9, 6, false);
    Geo_Lathe(g, MAT_EMBER_GLOW, (Vector3){ 0 }, er, ey, 1, 10, true);
}

/* 5 - The Thorned Crown: black iron band of twisted thorns, one blood-red gem at the front. */
static void BuildCrown(Geo *g)
{
    const float br[2] = { 0.19f, 0.19f }, by[2] = { -0.06f, 0.05f };
    const float ir[2] = { 0.17f, 0.17f }, iy[2] = { 0.05f, -0.06f };
    int i;
    Geo_Lathe(g, MAT_METAL, (Vector3){ 0 }, br, by, 2, 12, false);
    Geo_Lathe(g, MAT_METAL, (Vector3){ 0 }, ir, iy, 2, 12, false);                     /* inside */
    for (i = 0; i < 12; i++) {
        float a = 2.0f * PI * i / 12.0f, h = (i & 1) ? 0.12f : 0.2f;
        const float tr[2] = { 0.028f, 0.0f };
        float ty[2] = { 0.0f, h };
        Geo_Lathe(g, MAT_METAL, (Vector3){ cosf(a) * 0.18f, 0.05f, sinf(a) * 0.18f }, tr, ty, 2, 5, false);
        Geo_OBox(g, MAT_METAL, (Vector3){ cosf(a + 0.26f) * 0.195f, 0.0f, sinf(a + 0.26f) * 0.195f }, -a + 0.8f,
                 (Vector3){ 0.012f, 0.05f, 0.04f });                                       /* twisted thorns */
    }
    Gem(g, MAT_RUBY, (Vector3){ 0.0f, 0.02f, 0.2f }, 0.05f, 0.1f);
}

/* Unit meshes for the cage bars and the serpent (atlas UVs for the ember-scale body). */
static Mesh LatheTile(int tile, const float *r, const float *y, int rings, int sides)
{
    MeshBuilder mb;
    float u0, v0, u1, v1;
    Color white[4] = { WHITE, WHITE, WHITE, WHITE };
    int k, i;
    Textures_TileUV(tile, &u0, &v0, &u1, &v1);
    MB_Begin(&mb);
    for (k = 0; k + 1 < rings; k++)
        for (i = 0; i < sides; i++) {
            float a0 = 2.0f * PI * i / sides, a1 = 2.0f * PI * (i + 1) / sides;
            Vector3 p[4] = { { cosf(a1) * r[k], y[k], sinf(a1) * r[k] }, { cosf(a0) * r[k], y[k], sinf(a0) * r[k] },
                             { cosf(a0) * r[k + 1], y[k + 1], sinf(a0) * r[k + 1] }, { cosf(a1) * r[k + 1], y[k + 1], sinf(a1) * r[k + 1] } };
            Vector2 uv[4] = { { u0 + (u1 - u0) * (i + 1) / sides, v1 }, { u0 + (u1 - u0) * i / sides, v1 },
                              { u0 + (u1 - u0) * i / sides, v0 }, { u0 + (u1 - u0) * (i + 1) / sides, v0 } };
            MB_Quad(&mb, p, uv, white);
        }
    return MB_End(&mb);
}

static void BuildUnits(void)
{
    const float cr[2] = { 0.5f, 0.5f }, cy[2] = { -0.5f, 0.5f };
    float sr[7], sy[7];
    Geo g;
    int k;
    for (k = 0; k < 7; k++) { float a = PI * k / 6.0f; sr[k] = 0.5f * sinf(a); sy[k] = -0.5f * cosf(a); }
    unitCyl = LatheTile(TILE_IRON, cr, cy, 2, 6);
    unitSphere = LatheTile(TILE_WHITE, sr, sy, 7, 8);
    emberSphere = LatheTile(TILE_EMBER, sr, sy, 7, 8);
    {
        MeshBuilder mb;
        MB_Begin(&mb);
        MB_Box(&mb, (Vector3){ -0.5f, -0.5f, -0.5f }, (Vector3){ 0.5f, 0.5f, 0.5f }, TILE_WHITE, WHITE);
        crackBox = MB_End(&mb);
    }
    /* the dais: two stone steps */
    Begin(&g);
    {
        const float r[5] = { 2.0f, 2.0f, 1.75f, 1.75f, 0.0f }, y[5] = { 0.0f, 0.16f, 0.16f, 0.32f, 0.32f };
        Geo_Lathe(&g, MAT_TRIM, (Vector3){ 0 }, r, y, 5, 16, false);
    }
    Flush(&g, &cageDais);
}

void Relics_Init(void)
{
    static void (*const builders[WING_COUNT])(Geo *) = { BuildGrimoire, BuildRing, BuildLocket, BuildChalice, BuildCrown };
    Geo g;
    int i;
    for (i = 0; i < WING_COUNT; i++) {
        Begin(&g);
        builders[i](&g);
        Flush(&g, &relicMesh[i]);
    }
    BuildUnits();
}

void Relics_Shutdown(void)
{
    int i, k;
    for (i = 0; i < WING_COUNT; i++)
        for (k = 0; k < relicMesh[i].count; k++) UnloadMesh(relicMesh[i].mesh[k]);
    for (k = 0; k < cageDais.count; k++) UnloadMesh(cageDais.mesh[k]);
    UnloadMesh(unitCyl);
    UnloadMesh(unitSphere);
    UnloadMesh(emberSphere);
    UnloadMesh(crackBox);
}

/* ============================================================ drawing helpers */

static bool IsGlowMat(int m) { return m == MAT_RUBY || m == MAT_GLASS || m == MAT_EMBER_GLOW; }

static void DrawParts(const PartSet *ps, Matrix m, Color tint)
{
    int k;
    for (k = 0; k < ps->count; k++) {
        Material mat = Render_Material(ps->mat[k]);
        mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;
        if (IsGlowMat(ps->mat[k])) Render_SetEmissive(true);
        DrawMesh(ps->mesh[k], mat, m);
        if (IsGlowMat(ps->mat[k])) Render_SetEmissive(false);
    }
}

/* Unit mesh `mesh` stretched between points a and b (thickness t). */
static void Stick(Mesh mesh, Vector3 a, Vector3 b, float t, Color col, bool glow)
{
    Vector3 d = Vector3Subtract(b, a), axis;
    float len = Vector3Length(d), angle;
    Matrix m;
    Material mat = Render_WorldMaterial();
    if (len < 0.001f) return;
    d = Vector3Scale(d, 1.0f / len);
    axis = Vector3CrossProduct((Vector3){ 0, 1, 0 }, d);
    angle = acosf(Clamp(d.y, -1.0f, 1.0f));
    m = MatrixScale(t, len, t);
    if (Vector3Length(axis) > 0.0001f) m = MatrixMultiply(m, MatrixRotate(Vector3Normalize(axis), angle));
    else if (d.y < 0.0f) m = MatrixMultiply(m, MatrixRotateX(PI));
    m = MatrixMultiply(m, MatrixTranslate((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f));
    mat.maps[MATERIAL_MAP_DIFFUSE].color = col;
    if (glow) Render_SetEmissive(true);
    DrawMesh(mesh, mat, m);
    if (glow) Render_SetEmissive(false);
}

static void Ball(Mesh mesh, Vector3 c, Vector3 size, Color col, int emissiveMode)
{
    Material mat = Render_WorldMaterial();
    mat.maps[MATERIAL_MAP_DIFFUSE].color = col;
    if (emissiveMode) Render_SetEmissiveMode(emissiveMode);
    DrawMesh(mesh, mat, MatrixMultiply(MatrixScale(size.x, size.y, size.z), MatrixTranslate(c.x, c.y, c.z)));
    if (emissiveMode) Render_SetEmissiveMode(0);
}

/* The ring and the locket are built lying flat and stand up when drawn. */
static Matrix RelicBase(int index)
{
    if (index == 1 || index == 2) return MatrixRotateX(PI * 0.5f);
    return MatrixIdentity();
}

void Relics_DrawRelic(int index, Vector3 pos, float time, int hits)
{
    static const char *const slotNames[WING_COUNT] = { "relic_grimoire", "relic_ring", "relic_locket", "relic_chalice", "relic_crown" };
    float pulse = 0.75f + 0.25f * sinf(time * (index == 1 ? 7.0f : 3.0f));       /* the ring beats like a heart */
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixMultiply(RelicBase(index), MatrixScale(1.35f, 1.35f, 1.35f)), MatrixRotateY(time * 0.9f)),
                              MatrixTranslate(pos.x, pos.y, pos.z));
    int k;
    if (!slotsAsked) {
        slotsAsked = true;
        for (k = 0; k < WING_COUNT; k++) relicSlot[k] = Model_Slot(slotNames[k], ROLE_H_RELIC, MODEL_YAW_OFFSET);
    }
    Render_UseEntityLight((Vector3){ 0.6f, 0.4f, 0.4f });
    if (!Model_Draw(relicSlot[index], (Vector3){ pos.x, pos.y - ROLE_H_RELIC * 0.5f, pos.z }, time * 0.9f, ANIM_IDLE, time, WHITE))
        DrawParts(&relicMesh[index], m, (Color){ 255, (unsigned char)(255 * (0.8f + 0.2f * pulse)), (unsigned char)(255 * (0.8f + 0.2f * pulse)), 255 });
    /* glowing red cracks, more with every hit */
    for (k = 0; k < hits * 4; k++) {
        float a = Hash01(index, k, 1) * 2.0f * PI, b = (Hash01(index, k, 2) - 0.5f) * 2.0f;
        Vector3 c = { pos.x + cosf(a + time * 0.9f) * 0.24f, pos.y + b * 0.16f, pos.z - sinf(a + time * 0.9f) * 0.24f };
        Material mat = Render_WorldMaterial();
        mat.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 255, 60, 40, 255 };
        Render_SetEmissiveMode(2);
        DrawMesh(crackBox, mat, MatrixMultiply(MatrixMultiply(MatrixScale(0.025f, 0.16f, 0.025f), MatrixRotateZ(b * 1.2f)),
                                               MatrixMultiply(MatrixRotateY(a), MatrixTranslate(c.x, c.y, c.z))));
        Render_SetEmissiveMode(0);
    }
    Render_UseWorldLight();
}

/* ============================================================ the relic moment */

static Vector3 RelicCenter(const Game *g)
{
    float rise = fminf(1.0f, g->relicTime / 1.4f);
    return (Vector3){ g->relicPos.x, 0.55f + 0.85f * rise * (2.0f - rise) + 0.06f * sinf(g->relicTime * 2.0f), g->relicPos.z };
}

void Relics_Rise(Game *g, Vector3 chestPos)
{
    g->relicActive = true;
    g->relicPos = chestPos;
    g->relicTime = 0.0f;
    g->relicHits = 0;
    g->relicShake = 0.0f;
    Game_Banner(g, TextFormat("SOUL RELIC FOUND: %s", RELIC_NAMES[g->wing]), "Strike it with your wand!", COL_BLOOD, BANNER_TIME + 1.5f);
    Game_Particles(g, (Vector3){ chestPos.x, 0.8f, chestPos.z }, 40, (Color){ 255, 60, 40, 255 }, 2.4f, 0.06f, 1.2f, 6.0f);
    Audio_Play(SND_SCARE, 0.5f);
}

static void ShatterRelic(Game *g)
{
    static const char *const ord[WING_COUNT] = { "FIRST", "SECOND", "THIRD", "FOURTH", "FIFTH" };
    Vector3 c = RelicCenter(g);
    g->relicActive = false;
    g->relicDestroyed[g->wing] = true;
    Game_Particles(g, c, 70, (Color){ 255, 70, 50, 255 }, 4.0f, 0.07f, 1.6f, 6.0f);         /* red-white burst */
    Game_Particles(g, c, 40, (Color){ 255, 230, 220, 255 }, 3.0f, 0.05f, 1.0f, 3.0f);
    Game_Particles(g, c, 24, (Color){ 60, 50, 50, 255 }, 3.4f, 0.11f, 2.0f, 9.0f);           /* spinning shards */
    Game_Shake(g, 1.0f);
    g->redPulse = 1.0f;
    Audio_Play(SND_SCARE, 0.9f);
    Audio_Play(SND_BOLT_HIT, 1.0f);
    Audio_Play(SND_BELL_BREAK, 1.0f);
    Audio_Stop(SND_DRONE);
    Game_Banner(g, TextFormat("THE %s BELL SHATTERS", ord[g->wing]),
                g->wing == WING_COUNT - 1 ? "" : "The way forward is open...", COL_BLOOD, BANNER_TIME + 0.8f);
    if (g->wing == WING_COUNT - 1 && g->serpent.exists)
        Game_Banner(g, "THE CAGE SEAL IS BROKEN", "Break the cage and face the serpent", COL_GOLD, BANNER_TIME + 1.0f);
    Game_CheckWingComplete(g);
}

/* ============================================================ the cage + the serpent */

static float CageHeight(void) { return 3.3f; }

static bool CageSealed(const Game *g)
{
    int i;
    for (i = 0; i < WING_COUNT; i++) if (!g->relicDestroyed[i]) return true;
    return false;
}

static void PlaceSerpentInCage(Serpent *s, Vector3 c)
{
    int i;
    for (i = 0; i < SERPENT_SEGMENTS; i++) {
        float a = i * 0.45f;
        s->seg[i] = (Vector3){ c.x + cosf(a) * (0.35f + i * 0.025f), 0.45f, c.z + sinf(a) * (0.35f + i * 0.025f) };
    }
    s->headY = 0.45f;
}

static void SetAbbotShield(Game *g, bool on)
{
    int i;
    for (i = 0; i < g->enemyCount; i++)
        if (g->enemies[i].type == EN_ABBOT) {
            g->enemies[i].immortal = on;
            if (on) g->enemies[i].alerted = false;
        }
}

void Relics_OnWingLoaded(Game *g)
{
    Serpent *s = &g->serpent;
    g->relicActive = false;
    g->relicHits = 0;
    g->cageExists = g->world.hasCage;
    g->cagePos = g->world.cagePos;
    g->cageBroken = false;
    g->cageHits = 0;
    g->cageBurst = 0.0f;
    g->cageHint = false;
    g->shieldHint = false;
    memset(s, 0, sizeof(*s));
    s->exists = g->cageExists;
    if (!s->exists) return;
    s->alive = true;
    s->hp = SERPENT_HP;
    s->rear = -1.0f;
    s->lungeTimer = SERPENT_LUNGE_TIME;
    s->side = 1.0f;
    PlaceSerpentInCage(s, g->cagePos);
    SetAbbotShield(g, true);
    if (g->world.cageCollider >= 0)                    /* the bars are solid until broken */
        g->world.colliders[g->world.cageCollider] = (Collider){ g->cagePos.x - 1.4f, g->cagePos.z - 1.4f, g->cagePos.x + 1.4f, g->cagePos.z + 1.4f };
}

static void BreakCage(Game *g)
{
    Serpent *s = &g->serpent;
    g->cageBroken = true;
    g->cageBurst = 0.001f;
    if (g->world.cageCollider >= 0) g->world.colliders[g->world.cageCollider] = (Collider){ 0, 0, 0, 0 };
    s->freed = true;
    s->lungeTimer = SERPENT_LUNGE_TIME;
    Game_Particles(g, (Vector3){ g->cagePos.x, 1.6f, g->cagePos.z }, 60, (Color){ 255, 160, 60, 255 }, 4.0f, 0.06f, 1.2f, 6.0f);
    Game_Shake(g, 1.0f);
    Audio_Play(SND_HIT, 1.0f);
    Audio_Play(SND_DOOR, 1.0f);
    Audio_Play(SND_HISS, 1.0f);
    Game_Banner(g, "THE EMBER SERPENT IS FREE", "Dodge sideways when it rears up and glows red", COL_BLOOD, BANNER_TIME + 1.0f);
}

static void KillSerpent(Game *g)
{
    Serpent *s = &g->serpent;
    int i;
    s->alive = false;
    s->dying = 0.0f;
    for (i = 0; i < SERPENT_SEGMENTS; i += 2)
        Game_Particles(g, (Vector3){ s->seg[i].x, s->seg[i].y + 0.2f, s->seg[i].z }, 6, (Color){ 255, 120, 30, 255 }, 2.5f, 0.06f, 1.4f, 3.0f);
    Game_Shake(g, 0.9f);
    Audio_Play(SND_DEATH, 1.0f);
    g->enemiesSlain++;
    SetAbbotShield(g, false);
    for (i = 0; i < g->enemyCount; i++)
        if (g->enemies[i].alive && g->enemies[i].type == EN_ABBOT) {
            g->enemies[i].alerted = true;
            g->abbotAlerted = true;
            Game_Particles(g, (Vector3){ g->enemies[i].pos.x, 2.0f, g->enemies[i].pos.z }, 50, (Color){ 255, 50, 40, 255 }, 3.5f, 0.07f, 1.2f, 6.0f);
        }
    Game_Banner(g, "THE RED ABBOT IS MORTAL", "His shield is broken - destroy him!", COL_BLOOD, BANNER_TIME + 1.0f);
    Game_CheckWingComplete(g);
}

/* Rope-like body: every segment keeps its distance to the one in front. */
static void FollowBody(Serpent *s)
{
    int i;
    for (i = 1; i < SERPENT_SEGMENTS; i++) {
        Vector3 d = Vector3Subtract(s->seg[i], s->seg[i - 1]);
        float len = Vector3Length(d);
        if (len > SERPENT_SPACING) s->seg[i] = Vector3Add(s->seg[i - 1], Vector3Scale(d, SERPENT_SPACING / len));
        s->seg[i].y += (0.22f - s->seg[i].y) * 0.3f;          /* the body lies on the floor */
    }
}

static void UpdateSerpent(Game *g, float dt)
{
    Serpent *s = &g->serpent;
    Vector3 head = s->seg[0], to = Vector3Subtract(g->player.pos, head);
    float dist, wantY = 0.45f;
    to.y = 0.0f;
    dist = Vector3Length(to);
    s->time += dt;
    if (s->flash > 0.0f) s->flash -= dt;
    if (s->hissTimer > 0.0f) s->hissTimer -= dt;
    if (!s->alive) return;

    if (!s->freed) {
        /* coiled in the cage; raises its head and hisses when Kael comes near */
        float a = s->time * 0.8f;
        Vector3 target = { g->cagePos.x + cosf(a) * 0.55f, 0.45f, g->cagePos.z + sinf(a) * 0.55f };
        if (dist < 5.0f) {
            wantY = 1.25f;
            target = Vector3Add(g->cagePos, Vector3Scale(Vector3Normalize(to), 0.6f));
            if (s->hissTimer <= 0.0f) { Audio_Play(SND_HISS, 0.8f); s->hissTimer = 4.0f; }
        }
        if (g->cageHitTime > 0.0f) target = Vector3Add(target, (Vector3){ sinf(s->time * 30.0f) * 0.3f, 0, cosf(s->time * 27.0f) * 0.3f });   /* thrashing */
        s->seg[0] = Vector3Lerp(s->seg[0], target, fminf(1.0f, 3.0f * dt));
    } else if (s->rear >= 0.0f) {
        /* rearing up, glowing red: the telegraph */
        s->rear += dt;
        wantY = 1.6f;
        s->yaw = atan2f(to.x, to.z);
        if (s->rear >= SERPENT_REAR_TIME) {
            s->rear = -1.0f;
            s->lunge = SERPENT_LUNGE_DIST / SERPENT_LUNGE_SPEED;
            s->lungeDir = dist > 0.01f ? Vector3Scale(to, 1.0f / dist) : (Vector3){ 0, 0, 1 };
            s->lungeHit = false;
            Audio_Play(SND_HISS, 1.0f);
        }
    } else if (s->lunge > 0.0f) {
        /* striking forward */
        s->lunge -= dt;
        wantY = 0.7f;
        World_Move(&g->world, &s->seg[0], 0.6f, Vector3Scale(s->lungeDir, SERPENT_LUNGE_SPEED * dt));
        if (!s->lungeHit && Vector3Distance((Vector3){ s->seg[0].x, 0, s->seg[0].z }, (Vector3){ g->player.pos.x, 0, g->player.pos.z }) < SERPENT_HIT_RANGE) {
            s->lungeHit = true;
            Game_HurtPlayer(g, s->seg[0]);
        }
        if (s->lunge <= 0.0f) s->lungeTimer = SERPENT_LUNGE_TIME;
    } else {
        /* slither in curves around Kael, keeping its distance */
        Vector3 away = dist > 0.01f ? Vector3Scale(to, -1.0f / dist) : (Vector3){ 1, 0, 0 };
        Vector3 orbit = { away.x * cosf(0.7f * s->side) - away.z * sinf(0.7f * s->side), 0.0f,
                          away.x * sinf(0.7f * s->side) + away.z * cosf(0.7f * s->side) };
        Vector3 target = Vector3Add(g->player.pos, Vector3Scale(orbit, SERPENT_KEEP_DIST));
        Vector3 dir = Vector3Subtract(target, s->seg[0]);
        float want, len;
        dir.y = 0.0f;
        len = Vector3Length(dir);
        if (len > 0.2f) {
            want = atan2f(dir.x, dir.z);
            s->yaw += Clamp(Wrap(want - s->yaw, -PI, PI), -3.0f * dt, 3.0f * dt) + sinf(s->time * 3.0f) * 1.2f * dt;   /* curvy */
            World_Move(&g->world, &s->seg[0], 0.6f, (Vector3){ sinf(s->yaw) * SERPENT_SPEED * dt, 0, cosf(s->yaw) * SERPENT_SPEED * dt });
        }
        if (Hash01((int)(s->time * 0.3f), 1, 3) > 0.7f) s->side = -s->side;
        s->lungeTimer -= dt;
        if (s->lungeTimer <= 0.0f && dist < 9.0f &&
            World_LineOfSight(&g->world, (Vector3){ s->seg[0].x, 1.0f, s->seg[0].z }, (Vector3){ g->player.pos.x, 1.0f, g->player.pos.z }))
            s->rear = 0.0f;
    }
    s->headY += (wantY - s->headY) * fminf(1.0f, 6.0f * dt);
    s->seg[0].y = s->headY;
    FollowBody(s);
}

void Relics_Update(Game *g, float dt)
{
    if (g->relicActive) {
        g->relicTime += dt;
        if (g->relicShake > 0.0f) g->relicShake -= dt;
        if (!g->autotest) Audio_Loop(SND_DRONE, 0.7f);
        if (g->relicTime > 1.0f && fmodf(g->relicTime, 0.25f) < dt) {      /* wisps, embers or sparks */
            Vector3 c = RelicCenter(g);
            Color col = g->wing == 0 ? (Color){ 40, 36, 40, 255 } : (g->wing == 3 ? (Color){ 255, 130, 40, 255 }
                      : (g->wing == 2 ? (Color){ 150, 190, 255, 255 } : (Color){ 255, 50, 40, 255 }));
            Game_Particles(g, c, 2, col, 0.4f, 0.04f, 1.4f, g->wing == 0 || g->wing == 3 ? -0.6f : 1.0f);
        }
    }
    if (g->cageHitTime > 0.0f) g->cageHitTime -= dt;
    if (g->cageBurst > 0.0f && g->cageBurst < 3.0f) g->cageBurst += dt;
    if (g->serpent.exists) UpdateSerpent(g, dt);
}

/* ============================================================ aiming + hits */

int Relics_AimTarget(const Game *g, Vector3 from, Vector3 *pos)
{
    const Serpent *s = &g->serpent;
    if (g->relicActive) {
        Vector3 c = RelicCenter(g);
        if (Vector3Distance(from, c) < AIM_ASSIST_RANGE && World_LineOfSight(&g->world, from, c)) { *pos = c; return 1; }
    }
    if (s->exists && s->freed && s->alive) {
        Vector3 c = { s->seg[0].x, s->seg[0].y, s->seg[0].z };
        if (Vector3Distance(from, c) < AIM_ASSIST_RANGE && World_LineOfSight(&g->world, from, c)) { *pos = c; return 3; }
    }
    if (g->cageExists && !g->cageBroken && !CageSealed(g)) {
        Vector3 c = { g->cagePos.x, 1.4f, g->cagePos.z };
        if (Vector3Distance(from, c) < AIM_ASSIST_RANGE + 1.0f && World_LineOfSight(&g->world, from, c)) { *pos = c; return 2; }
    }
    return 0;
}

static void Sparks(Game *g, Vector3 p, Color c)
{
    Game_Particles(g, p, 16, c, 2.6f, 0.04f, 0.5f, 6.0f);
}

int Relics_RayHit(Game *g, Vector3 p)
{
    Serpent *s = &g->serpent;
    int i;
    if (g->relicActive && Vector3Distance(p, RelicCenter(g)) < 0.45f) {
        g->relicHits++;
        g->relicShake = 0.25f;
        Sparks(g, p, (Color){ 255, 80, 60, 255 });
        Audio_Play(SND_SCARE, 0.6f);                         /* the relic screams */
        Game_Shake(g, 0.35f);
        if (g->relicHits >= RELIC_HITS) ShatterRelic(g);
        return 1;
    }
    if (g->cageExists && !g->cageBroken) {
        float dx = p.x - g->cagePos.x, dz = p.z - g->cagePos.z;
        if (dx * dx + dz * dz < 1.35f * 1.35f && p.y < CageHeight() + 0.8f) {
            if (CageSealed(g)) {
                Sparks(g, p, (Color){ 255, 60, 40, 255 });
                if (!g->cageHint) {
                    g->cageHint = true;
                    Game_Banner(g, "The cage is sealed", "Destroy all five Soul Relics to break its seal", COL_BLOOD, BANNER_TIME);
                }
                return 1;
            }
            g->cageHits++;
            g->cageHitTime = 0.4f;
            Sparks(g, p, (Color){ 255, 200, 120, 255 });
            Audio_Play(SND_HIT, 1.0f);                       /* metal clang */
            Game_Shake(g, 0.4f);
            if (g->cageHits >= CAGE_HITS) BreakCage(g);
            return 1;
        }
    }
    if (s->exists && s->freed && s->alive) {
        for (i = 0; i < SERPENT_SEGMENTS; i += 3) {
            if (Vector3Distance(p, s->seg[i]) < (i == 0 ? 0.6f : 0.42f)) {
                s->hp--;
                s->flash = 0.15f;
                Sparks(g, p, (Color){ 255, 130, 40, 255 });
                Audio_Play(SND_HISS, 0.7f);
                if (s->hp <= 0) KillSerpent(g);
                return 1;
            }
        }
    }
    for (i = 0; i < g->enemyCount; i++) {                     /* the shielded Abbot: sparks */
        const Enemy *e = &g->enemies[i];
        if (e->alive && e->immortal && Vector3Distance(p, (Vector3){ e->pos.x, 1.9f, e->pos.z }) < 1.7f) {
            Sparks(g, p, (Color){ 255, 50, 40, 255 });
            if (!g->shieldHint) {
                g->shieldHint = true;
                Game_Banner(g, "The Red Abbot cannot be hurt", "His life is bound to the serpent in the cage", COL_BLOOD, BANNER_TIME);
            }
            return 1;
        }
    }
    return 0;
}

void Relics_Complete(Game *g)
{
    if (g->relicActive) { g->relicHits = RELIC_HITS; ShatterRelic(g); }
    g->relicDestroyed[g->wing] = true;
    if (g->serpent.exists && !g->cageBroken) { BreakCage(g); }
    if (g->serpent.exists && g->serpent.alive) KillSerpent(g);
}

/* ============================================================ lights + drawing */

void Relics_Lights(const Game *g)
{
    if (g->relicActive) {
        Vector3 c = RelicCenter(g);
        Render_AddDynamicLight(c, (Vector3){ 1.0f, 0.12f, 0.08f }, 5.0f);
        Render_SetFlare(-0.45f);                                        /* nearby candles dim */
    }
    if (g->cageExists && !g->cageBroken)
        Render_AddDynamicLight((Vector3){ g->cagePos.x, 1.0f, g->cagePos.z },
                               CageSealed(g) ? (Vector3){ 0.9f, 0.1f, 0.06f } : (Vector3){ 0.6f, 0.3f, 0.1f }, 6.0f);
    if (g->serpent.exists && g->serpent.alive && g->serpent.freed)
        Render_AddDynamicLight(g->serpent.seg[4], (Vector3){ 0.9f, 0.35f, 0.08f }, 4.0f);
}

static void DrawCage(const Game *g)
{
    const int bars = 8;
    Vector3 c = g->cagePos;
    float top = CageHeight(), seal = CageSealed(g) ? 1.0f : 0.0f;
    float ceil = World_CeilingAt(&g->world, c.x, c.z);
    int i;
    Matrix m = MatrixTranslate(c.x, 0.0f, c.z);
    Render_UseEntityLight((Vector3){ 0.4f, 0.3f, 0.3f });
    DrawParts(&cageDais, m, WHITE);
    /* glowing red runes on the dais rim (fade once the seal is broken) */
    for (i = 0; i < 12 && seal > 0.0f; i++) {
        float a = 2.0f * PI * i / 12.0f + 0.2f;
        Vector3 p = { c.x + cosf(a) * 1.85f, 0.18f, c.z + sinf(a) * 1.85f };
        Ball(unitSphere, p, (Vector3){ 0.12f, 0.06f, 0.12f }, (Color){ 255, 40, 30, 255 }, 2);
    }
    for (i = 0; i < bars; i++) {
        float a = 2.0f * PI * i / bars;
        Vector3 dir = { cosf(a), 0.0f, sinf(a) };
        float bend = g->cageHits * 0.05f * (0.5f + Hash01(i, 1, 9));
        Vector3 lo = Vector3Add(c, Vector3Scale(dir, 1.25f)), hi = Vector3Add(c, Vector3Scale(dir, 1.1f)), mid;
        lo.y = 0.32f;
        hi.y = top;
        mid = Vector3Add(Vector3Lerp(lo, hi, 0.5f), Vector3Scale(dir, bend));
        if (g->cageHitTime > 0.0f) mid = Vector3Add(mid, (Vector3){ sinf(g->time * 60.0f + i) * 0.04f, 0, 0 });
        if (g->cageBroken) {
            /* bars burst outward and fall */
            float t = g->cageBurst;
            Vector3 fly = Vector3Add(Vector3Scale(dir, t * 5.0f), (Vector3){ 0, 3.0f * t - 6.0f * t * t, 0 });
            if (t > 1.2f) continue;
            lo = Vector3Add(lo, fly); hi = Vector3Add(hi, Vector3Add(fly, Vector3Scale(dir, t * 2.0f))); mid = Vector3Lerp(lo, hi, 0.5f);
        }
        Stick(unitCyl, lo, mid, 0.07f, (Color){ 120, 110, 105, 255 }, false);
        Stick(unitCyl, mid, hi, 0.07f, (Color){ 120, 110, 105, 255 }, false);
        if (!g->cageBroken) {
            Vector3 apex = { c.x, top + 0.9f, c.z };
            Stick(unitCyl, hi, Vector3Lerp(hi, apex, 0.55f), 0.07f, (Color){ 120, 110, 105, 255 }, false);   /* dome ribs */
            Stick(unitCyl, Vector3Lerp(hi, apex, 0.55f), apex, 0.07f, (Color){ 120, 110, 105, 255 }, false);
        }
    }
    if (!g->cageBroken) {
        for (i = 0; i < 3; i++) {                                         /* chains to the ceiling */
            float a = i * 2.0f * PI / 3.0f;
            Stick(unitCyl, (Vector3){ c.x + cosf(a) * 0.2f, top + 0.9f, c.z + sinf(a) * 0.2f },
                  (Vector3){ c.x + cosf(a) * 1.4f, ceil, c.z + sinf(a) * 1.4f }, 0.04f, (Color){ 70, 66, 64, 255 }, false);
        }
    }
    Render_UseWorldLight();
}

static void DrawSerpent(const Game *g)
{
    const Serpent *s = &g->serpent;
    float glow = s->rear >= 0.0f ? fminf(1.0f, s->rear / SERPENT_REAR_TIME) : 0.0f;
    int i;
    if (!s->alive) return;
    Render_UseEntityLight((Vector3){ 0.5f, 0.35f, 0.3f });
    for (i = SERPENT_SEGMENTS - 1; i >= 0; i--) {
        float t = (float)i / SERPENT_SEGMENTS, r = (i == 0 ? 0.3f : 0.26f * (1.0f - t * 0.75f) + 0.05f);
        Vector3 p = s->seg[i];
        Color col = { (unsigned char)(150 + 105 * glow), (unsigned char)(118 - 80 * glow), (unsigned char)(104 - 70 * glow), 255 };
        if (s->flash > 0.0f) col = WHITE;
        p.y += sinf(s->time * 6.0f - i * 0.6f) * 0.04f;
        Ball(emberSphere, p, (Vector3){ r * 2.0f, r * 1.7f, r * 2.0f }, col, 0);
        if (i > 0 && i < SERPENT_SEGMENTS - 1) {
            Vector3 n = s->seg[i - 1];
            Stick(unitCyl, p, n, r * 1.6f, col, false);
        }
    }
    {
        /* head: snout, burning red eyes, flicking tongue */
        Vector3 h = s->seg[0], d = Vector3Subtract(s->seg[0], s->seg[1]), side;
        d.y = 0.0f;
        d = Vector3Length(d) > 0.001f ? Vector3Normalize(d) : (Vector3){ 0, 0, 1 };
        side = (Vector3){ d.z, 0.0f, -d.x };
        Ball(emberSphere, Vector3Add(h, Vector3Scale(d, 0.22f)), (Vector3){ 0.32f, 0.24f, 0.32f }, (Color){ 60, 50, 48, 255 }, 0);
        Ball(unitSphere, Vector3Add(Vector3Add(h, Vector3Scale(d, 0.14f)), Vector3Add(Vector3Scale(side, 0.13f), (Vector3){ 0, 0.1f, 0 })),
             (Vector3){ 0.08f, 0.06f, 0.08f }, (Color){ 255, 30, 20, 255 }, 2);
        Ball(unitSphere, Vector3Add(Vector3Add(h, Vector3Scale(d, 0.14f)), Vector3Add(Vector3Scale(side, -0.13f), (Vector3){ 0, 0.1f, 0 })),
             (Vector3){ 0.08f, 0.06f, 0.08f }, (Color){ 255, 30, 20, 255 }, 2);
        if (fmodf(s->time, 1.3f) < 0.25f)
            Stick(unitCyl, Vector3Add(h, Vector3Scale(d, 0.36f)), Vector3Add(h, Vector3Scale(d, 0.58f)), 0.02f, (Color){ 200, 30, 40, 255 }, true);
    }
    Render_UseWorldLight();
}

void Relics_Draw(const Game *g)
{
    if (g->relicActive) {
        Vector3 c = RelicCenter(g);
        if (g->relicShake > 0.0f) c = Vector3Add(c, (Vector3){ sinf(g->time * 80.0f) * 0.05f, 0, cosf(g->time * 70.0f) * 0.05f });
        Relics_DrawRelic(g->wing, c, g->relicTime, g->relicHits);
    }
    if (g->cageExists) DrawCage(g);
    if (g->serpent.exists) DrawSerpent(g);
}

void Relics_DrawTransparent(const Game *g)
{
    int i;
    for (i = 0; i < g->enemyCount; i++) {
        const Enemy *e = &g->enemies[i];
        float p = 0.5f + 0.5f * sinf(g->time * 3.0f);
        if (!e->alive || !e->immortal) continue;
        DrawSphereEx((Vector3){ e->pos.x, 1.9f, e->pos.z }, 1.75f, 10, 12, (Color){ 200, 20, 20, (unsigned char)(40 + 30 * p) });
        DrawSphereWires((Vector3){ e->pos.x, 1.9f, e->pos.z }, 1.78f, 8, 10, (Color){ 255, 60, 40, (unsigned char)(60 + 50 * p) });
    }
}
