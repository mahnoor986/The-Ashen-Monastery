/* title.c - the title screen (see title.h).
 * The scene: a jagged snowy cliff and an original castle-monastery on top (round towers of
 * different heights with conical spires and needle tips, curtain walls with battlements, a long
 * hall with pointed windows and buttresses, many lit windows), a stormy sky with a pale moon and
 * drifting clouds, falling snow, mist at the cliff's foot, crows circling the spires, lightning,
 * and two big coloured lights (warm orange, cold teal) sweeping across the walls. The camera
 * drifts slowly: low and looking up, rising and orbiting a little, in a smooth loop. */
#include <math.h>
#include <string.h>
#include "title.h"
#include "game.h"
#include "geo.h"
#include "render.h"
#include "audio.h"
#include "ui.h"
#include "config.h"
#include "raymath.h"
#include "rlgl.h"

#define MAX_TITLE_PARTS  MAT_COUNT
#define CLIFF_H          28.0f
#define CLOUDS           14
#define SNOWFLAKES       420
#define CROWS            7

static Mesh      parts[MAX_TITLE_PARTS];    /* module-private GPU resources: one mesh per material */
static int       partMat[MAX_TITLE_PARTS], partCount;
static Texture2D cloudTex, moonTex;
static float     lightningTimer = 3.0f, lightningAge = -1.0f;
static bool      thunderDone = true;

static const WingConfig TITLE_LOOK = { "", 0, 1.0f, 1.0f, 1.0f, 0.0042f, 0.01f, 0.30f,
                                       { 0.04f, 0.05f, 0.08f }, { 0.55f, 0.65f, 1.0f }, 1.0f };

typedef struct { float x, z, r, h, spire; int sides; } Tower;

static const Tower TOWERS[] = {
    {   0.0f,   0.0f, 5.5f, 25.0f, 20.0f, 12 },     /* the great keep */
    { -11.0f,   4.0f, 3.4f, 19.0f, 14.0f, 10 },
    {  10.5f,  -3.0f, 3.1f, 22.0f, 16.0f, 10 },
    {  -6.5f, -10.0f, 2.8f, 15.0f, 12.0f, 8 },
    {   7.0f,   9.5f, 2.6f, 13.0f, 11.0f, 8 },
    {  14.0f,   7.0f, 2.2f, 17.0f, 13.0f, 8 },
    { -14.0f,  -6.0f, 2.4f, 12.0f, 10.0f, 8 },
    {   2.5f, -13.0f, 2.0f, 16.0f, 12.0f, 8 },
};
#define TOWER_COUNT ((int)(sizeof(TOWERS) / sizeof(TOWERS[0])))

/* ============================================================ helpers */

static float Hash01(int x, int y, int seed)
{
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ (unsigned int)seed * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
}

/* Lightning brightness 0..1 (two quick flashes, as in the wings). */
static float Flash(void)
{
    float t = lightningAge;
    if (t < 0.0f) return 0.0f;
    return fminf(1.0f, fmaxf(0.0f, 1.0f - fabsf(t - 0.05f) / 0.09f) + 0.85f * fmaxf(0.0f, 1.0f - fabsf(t - 0.32f) / 0.1f));
}

/* ============================================================ the cliff */

static void BuildCliff(Geo *g)
{
    const int rings = 10, sides = 26;
    Vector3 p[11][27];
    int k, i;
    for (k = 0; k <= rings; k++) {
        float t = (float)k / rings;
        for (i = 0; i <= sides; i++) {
            int ii = i % sides;
            float a = 2.0f * PI * ii / sides;
            float r = 17.5f + (50.0f - 17.5f) * powf(1.0f - t, 1.3f);
            float y = k == 0 ? -8.0f : t * CLIFF_H;
            if (k > 0 && k < rings) {           /* jagged ledges and crags */
                r += (Hash01(k, ii, 1) - 0.4f) * 9.0f * (1.0f - t * 0.5f) + (ii % 5 == 0 ? 4.0f : 0.0f);
                y += (Hash01(k, ii, 2) - 0.5f) * 2.0f;
            }
            if (k == rings) r += (Hash01(k, ii, 3) - 0.5f) * 1.5f;
            p[k][i] = (Vector3){ cosf(a) * r, y, sinf(a) * r };
        }
    }
    for (k = 0; k < rings; k++) {
        for (i = 0; i < sides; i++) {
            Vector3 a = p[k][i + 1], b = p[k][i], c = p[k + 1][i], d = p[k + 1][i + 1];
            Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
            /* snow settles on the gentler upper slopes and ledges; the steep lower crags stay bare */
            bool snow = (k >= rings / 2 && n.y > 0.3f) || n.y > 0.7f;
            Geo_Quad2(g, snow ? MAT_SNOW : MAT_PILLAR, a, b, c, d);
        }
    }
    for (i = 0; i < sides; i++)                                          /* snowy plateau */
        Geo_Quad(g, MAT_SNOW, p[rings][i + 1], p[rings][i], (Vector3){ 0, CLIFF_H, 0 }, (Vector3){ 0, CLIFF_H, 0 });
    /* dark snowfield far below, fading into the mist */
    Geo_Quad(g, MAT_SNOW, (Vector3){ -260, -8, 260 }, (Vector3){ 260, -8, 260 }, (Vector3){ 260, -8, -260 }, (Vector3){ -260, -8, -260 });
}

/* ============================================================ the castle */

/* A pointed window quad (+ tip) on a vertical face: centre c, facing n, width w, height h. */
static void CastleWindow(Geo *g, Vector3 c, Vector3 n, float w, float h, int mat)
{
    Vector3 s = { n.z * w * 0.5f, 0.0f, -n.x * w * 0.5f };
    Vector3 o = Vector3Add(c, Vector3Scale(n, 0.06f));
    Vector3 a = { o.x - s.x, o.y - h * 0.5f, o.z - s.z }, b = { o.x + s.x, o.y - h * 0.5f, o.z + s.z };
    Vector3 cc = { b.x, o.y + h * 0.3f, b.z }, d = { a.x, o.y + h * 0.3f, a.z }, tip = { o.x, o.y + h * 0.55f, o.z };
    Vector3 q[4] = { a, b, cc, d };
    const Vector2 uv[4] = { { 0, 1 }, { 1, 1 }, { 1, 0.2f }, { 0, 0.2f } };
    Vector3 t[4] = { d, cc, tip, tip };
    const Vector2 tuv[4] = { { 0, 0.2f }, { 1, 0.2f }, { 0.5f, 0.0f }, { 0.5f, 0.0f } };
    Geo_QuadUV(g, mat, q, uv, false);
    Geo_QuadUV(g, mat, t, tuv, false);
}

static int WindowMat(int a, int b)
{
    float r = Hash01(a, b, 9);
    return r < 0.62f ? MAT_WINDOW_LIT : (r < 0.86f ? MAT_WINDOW_FLICKER : MAT_GLASS);
}

static void BuildTower(Geo *g, const Tower *t, int index)
{
    Vector3 base = { t->x, CLIFF_H - 1.0f, t->z };
    const float wr[2] = { t->r, t->r }, wy[2] = { 0.0f, t->h + 1.0f };
    const float cr[3] = { t->r, t->r + 0.55f, t->r + 0.55f }, cy[3] = { t->h - 0.2f, t->h + 0.6f, t->h + 1.4f };
    const float sr[3] = { t->r + 0.5f, t->r * 0.35f, 0.06f }, sy[3] = { t->h + 1.4f, t->h + 1.4f + t->spire * 0.55f, t->h + 1.4f + t->spire };
    float faceR = t->r * cosf(PI / t->sides);
    int i;
    float y;
    Geo_Lathe(g, MAT_TRIM, base, wr, wy, 2, t->sides, false);             /* pale stone catches the moon */
    Geo_Lathe(g, MAT_WALL_STONE, base, cr, cy, 3, t->sides, false);
    Geo_Lathe(g, MAT_PILLAR, base, sr, sy, 3, t->sides, false);           /* conical spire */
    Geo_Box(g, MAT_METAL, (Vector3){ t->x - 0.08f, base.y + sy[2] - 0.2f, t->z - 0.08f },
            (Vector3){ t->x + 0.08f, base.y + sy[2] + 3.0f, t->z + 0.08f });   /* needle tip */
    for (y = 4.0f; y < t->h - 1.5f; y += 4.0f) {
        for (i = 0; i < t->sides; i++) {
            float a = 2.0f * PI * (i + 0.5f) / t->sides;
            Vector3 n = { cosf(a), 0.0f, sinf(a) };
            if (Hash01(index * 31 + i, (int)y, 7) > 0.78f) continue;
            CastleWindow(g, (Vector3){ t->x + n.x * faceR, base.y + y, t->z + n.z * faceR }, n, 0.9f, 1.7f, WindowMat(index * 31 + i, (int)y));
        }
    }
}

/* A curtain wall between two points with battlements along its top. */
static void CurtainWall(Geo *g, float x0, float z0, float x1, float z1, float h)
{
    float dx = x1 - x0, dz = z1 - z0, len = sqrtf(dx * dx + dz * dz), yaw = atan2f(dx, dz), k;
    Vector3 c = { (x0 + x1) * 0.5f, CLIFF_H - 1.0f + h * 0.5f, (z0 + z1) * 0.5f };
    Frame f = { { c.x, CLIFF_H - 1.0f, c.z }, yaw };
    Geo_OBox(g, MAT_WALL_STONE, c, yaw, (Vector3){ 0.9f, h * 0.5f, len * 0.5f });
    for (k = -len * 0.5f + 2.0f; k < len * 0.5f - 1.5f; k += 3.0f) {
        Vector3 n = { cosf(yaw), 0.0f, -sinf(yaw) };
        if (Hash01((int)(x0 * 7 + k), (int)z1, 5) < 0.5f)
            CastleWindow(g, Frame_Point(f, 0.9f, h * 0.55f, k), n, 0.7f, 1.4f, WindowMat((int)(k * 5), (int)x1));
        if (Hash01((int)(z0 * 7 + k), (int)x1, 6) < 0.5f)
            CastleWindow(g, Frame_Point(f, -0.9f, h * 0.55f, k), Vector3Negate(n), 0.7f, 1.4f, WindowMat((int)(k * 5), (int)z1));
    }
    for (k = -len * 0.5f + 0.6f; k < len * 0.5f - 0.4f; k += 1.5f) {
        Geo_FBox(g, MAT_WALL_STONE, f, -0.65f, h + 0.45f, k, 0.3f, 0.45f, 0.4f);   /* merlons both sides */
        Geo_FBox(g, MAT_WALL_STONE, f, 0.65f, h + 0.45f, k, 0.3f, 0.45f, 0.4f);
    }
}

/* The long hall: walls, a steep gable roof, tall pointed windows and buttresses. */
static void BuildHall(Geo *g)
{
    Frame f = { { 3.5f, CLIFF_H - 1.0f, 11.0f }, 0.28f };
    const float hw = 4.2f, hl = 8.5f, wallH = 9.0f, ridge = 5.0f;
    float z;
    Geo_FBox(g, MAT_TRIM, f, 0, wallH * 0.5f, 0, hw, wallH * 0.5f, hl);
    Geo_Quad(g, MAT_PILLAR, Frame_Point(f, -hw - 0.4f, wallH, hl + 0.3f), Frame_Point(f, -hw - 0.4f, wallH, -hl - 0.3f),
             Frame_Point(f, 0, wallH + ridge, -hl - 0.3f), Frame_Point(f, 0, wallH + ridge, hl + 0.3f));
    Geo_Quad(g, MAT_PILLAR, Frame_Point(f, hw + 0.4f, wallH, -hl - 0.3f), Frame_Point(f, hw + 0.4f, wallH, hl + 0.3f),
             Frame_Point(f, 0, wallH + ridge, hl + 0.3f), Frame_Point(f, 0, wallH + ridge, -hl - 0.3f));
    Geo_Quad(g, MAT_WALL_STONE, Frame_Point(f, -hw, wallH, hl), Frame_Point(f, hw, wallH, hl),
             Frame_Point(f, 0, wallH + ridge, hl), Frame_Point(f, 0, wallH + ridge, hl));            /* gable ends */
    Geo_Quad(g, MAT_WALL_STONE, Frame_Point(f, hw, wallH, -hl), Frame_Point(f, -hw, wallH, -hl),
             Frame_Point(f, 0, wallH + ridge, -hl), Frame_Point(f, 0, wallH + ridge, -hl));
    for (z = -hl + 1.5f; z < hl - 1.0f; z += 2.6f) {
        Vector3 nr = Vector3Normalize(Vector3Subtract(Frame_Point(f, 1, 0, 0), f.o));
        CastleWindow(g, Frame_Point(f, hw, 5.0f, z), nr, 1.0f, 3.2f, WindowMat((int)(z * 3), 1));
        CastleWindow(g, Frame_Point(f, -hw, 5.0f, z), Vector3Negate(nr), 1.0f, 3.2f, WindowMat((int)(z * 3), 2));
        Geo_FBox(g, MAT_WALL_STONE, f, hw + 0.45f, 3.5f, z + 1.3f, 0.45f, 3.5f, 0.35f);              /* buttresses */
        Geo_FBox(g, MAT_WALL_STONE, f, -hw - 0.45f, 3.5f, z + 1.3f, 0.45f, 3.5f, 0.35f);
    }
    CastleWindow(g, Frame_Point(f, 0, 7.5f, hl), Vector3Normalize(Vector3Subtract(Frame_Point(f, 0, 0, 1), f.o)), 2.2f, 4.0f, MAT_GLASS);   /* rose end window */
}

static void BuildCastle(Geo *g)
{
    int i;
    for (i = 0; i < TOWER_COUNT; i++) BuildTower(g, &TOWERS[i], i);
    for (i = 1; i < TOWER_COUNT; i++) {
        const Tower *a = &TOWERS[i], *b = &TOWERS[i + 1 < TOWER_COUNT ? i + 1 : 1];
        CurtainWall(g, a->x, a->z, b->x, b->z, 7.0f + (i % 3));
    }
    BuildHall(g);
}

/* ============================================================ sky textures */

static Texture2D MakeCloudTexture(void)
{
    Image img = GenImagePerlinNoise(128, 128, 0, 0, 3.5f);
    Color *px;
    int i, x, y;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    px = (Color *)img.data;
    for (y = 0; y < 128; y++) {
        for (x = 0; x < 128; x++) {
            float dx = (x - 63.5f) / 64.0f, dy = (y - 63.5f) / 64.0f, edge = fmaxf(0.0f, 1.0f - (dx * dx + dy * dy * 1.8f));
            i = y * 128 + x;
            px[i].a = (unsigned char)fminf(255.0f, fmaxf(0.0f, (px[i].r / 255.0f - 0.35f) * 2.2f) * edge * 255.0f);
            px[i].r = px[i].g = px[i].b = 255;
        }
    }
    {
        Texture2D t = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        return t;
    }
}

static Texture2D MakeMoonTexture(void)
{
    Image img = GenImageColor(64, 64, BLANK);
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            float dx = x - 31.5f, dy = y - 31.5f, r = sqrtf(dx * dx + dy * dy);
            float crater = Hash01(x / 6, y / 6, 4) > 0.75f ? 0.85f : 1.0f;
            if (r < 26.0f) ImageDrawPixel(&img, x, y, (Color){ (unsigned char)(220 * crater), (unsigned char)(226 * crater), 236, 255 });
            else if (r < 31.0f) ImageDrawPixel(&img, x, y, (Color){ 200, 210, 240, (unsigned char)(90 * (31.0f - r) / 5.0f) });
        }
    {
        Texture2D t = LoadTextureFromImage(img);
        UnloadImage(img);
        return t;
    }
}

/* ============================================================ API */

void Title_Init(void)
{
    static Geo g;
    int m;
    memset(&g, 0, sizeof(g));
    g.w = NULL;                    /* no baked light: the title is lit per pixel */
    for (m = 0; m < MAT_COUNT; m++) MB_Begin(&g.mb[m]);
    BuildCliff(&g);
    BuildCastle(&g);
    partCount = 0;
    for (m = 0; m < MAT_COUNT; m++) {
        Mesh mesh = MB_End(&g.mb[m]);
        if (mesh.vertexCount == 0) continue;
        parts[partCount] = mesh;
        partMat[partCount] = m;
        partCount++;
    }
    cloudTex = MakeCloudTexture();
    moonTex = MakeMoonTexture();
}

void Title_Shutdown(void)
{
    int i;
    for (i = 0; i < partCount; i++) UnloadMesh(parts[i]);
    partCount = 0;
    UnloadTexture(cloudTex);
    UnloadTexture(moonTex);
}

void Title_Flash(void)
{
    lightningAge = 0.0f;
    thunderDone = true;              /* a menu flash has no thunder */
}

void Title_Update(float dt, bool sounds)
{
    lightningTimer -= dt;
    if (lightningTimer <= 0.0f) {
        lightningTimer = TITLE_LIGHTNING_MIN + (TITLE_LIGHTNING_MAX - TITLE_LIGHTNING_MIN) * GetRandomValue(0, 100) / 100.0f;
        lightningAge = 0.0f;
        thunderDone = false;
    }
    if (lightningAge >= 0.0f) {
        lightningAge += dt;
        if (!thunderDone && lightningAge > 1.0f) { thunderDone = true; if (sounds) Audio_Play(SND_THUNDER, 1.0f); }
        if (lightningAge > 3.0f) lightningAge = -1.0f;
    }
    if (sounds) Audio_Loop(SND_WIND, 0.6f);
}

/* Slow cinematic drift: starts low looking up, rises and orbits a little, loops every 70 s. */
static Camera3D TitleCamera(float time)
{
    const float period = 70.0f;
    float u = 0.5f - 0.5f * cosf(2.0f * PI * time / period);
    float a = 0.55f + 0.14f * sinf(2.0f * PI * time / period);
    float dist = 84.0f - 10.0f * u;
    Camera3D cam = { 0 };
    cam.position = (Vector3){ sinf(a) * dist + 0.4f * sinf(time * 0.7f), 20.0f + 20.0f * u + 0.3f * sinf(time * 0.9f), cosf(a) * dist };
    cam.target = (Vector3){ 0.0f, CLIFF_H + 13.0f - 3.0f * u, 0.0f };
    cam.up = (Vector3){ 0, 1, 0 };
    cam.fovy = 46.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

static void DrawSky(Camera3D cam, float time, float flash)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position)), right = { fwd.z, 0, -fwd.x };
    int i;
    unsigned char c = (unsigned char)(96 + 120 * flash);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    DrawBillboard(cam, moonTex, Vector3Add(cam.position, Vector3Add(Vector3Scale(fwd, 300.0f),
                  (Vector3){ right.x * -70.0f, 105.0f, right.z * -70.0f })), 34.0f, WHITE);
    for (i = 0; i < CLOUDS; i++) {
        float lane = (float)i / CLOUDS, speed = 1.2f + 1.5f * Hash01(i, 1, 11);
        float side = fmodf(Hash01(i, 2, 12) * 500.0f + time * speed, 500.0f) - 250.0f;
        Vector3 p = Vector3Add(cam.position, Vector3Add(Vector3Scale(fwd, 230.0f + 80.0f * Hash01(i, 3, 13)),
                    (Vector3){ right.x * side, 50.0f + 110.0f * lane, right.z * side }));
        unsigned char a = (unsigned char)(150 + 80 * Hash01(i, 4, 14));
        DrawBillboard(cam, cloudTex, p, 110.0f + 90.0f * Hash01(i, 5, 15), (Color){ c, (unsigned char)(c + 6), (unsigned char)(c + 22), a });
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

/* Snow drifting through a box in front of the camera. */
static void DrawSnow(Camera3D cam, float time)
{
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 right = Vector3Normalize((Vector3){ fwd.z, 0.0f, -fwd.x });
    int i;
    for (i = 0; i < SNOWFLAKES; i++) {
        /* a box in front of the camera (camera space), 10..40 units away */
        float x = fmodf(Hash01(i, 1, 21) * 44.0f + time * 1.4f, 44.0f) - 22.0f;
        float y = 16.0f - fmodf(Hash01(i, 2, 22) * 32.0f + time * (1.6f + Hash01(i, 4, 24)), 32.0f);
        float z = 10.0f + Hash01(i, 3, 23) * 30.0f;
        Vector3 p = Vector3Add(cam.position, Vector3Add(Vector3Scale(fwd, z), Vector3Scale(right, x + sinf(time + i) * 0.4f)));
        p.y += y;
        DrawCube(p, 0.11f, 0.11f, 0.11f, (Color){ 220, 228, 240, 200 });
    }
}

/* Crows: two flapping wing triangles each, circling the spires. */
static void DrawCrows(float time)
{
    int i;
    for (i = 0; i < CROWS; i++) {
        const Tower *t = &TOWERS[i % TOWER_COUNT];
        float r = 9.0f + 5.0f * Hash01(i, 1, 31), w = (0.22f + 0.15f * Hash01(i, 2, 32)) * (i & 1 ? 1.0f : -1.0f);
        float a = time * w + i * 1.7f, y = CLIFF_H + t->h + t->spire * 0.6f + 3.0f * Hash01(i, 3, 33) + sinf(time * 0.7f + i) * 1.5f;
        Vector3 c = { t->x + cosf(a) * r, y, t->z + sinf(a) * r };
        Vector3 f = { -sinf(a) * (w > 0 ? 1.0f : -1.0f), 0.0f, cosf(a) * (w > 0 ? 1.0f : -1.0f) }, s = { f.z, 0.0f, -f.x };
        float flap = sinf(time * 9.0f + i * 2.0f) * 0.7f;
        Vector3 lt = { c.x + s.x * 1.3f, c.y + flap, c.z + s.z * 1.3f }, rt = { c.x - s.x * 1.3f, c.y + flap, c.z - s.z * 1.3f };
        Vector3 nose = Vector3Add(c, Vector3Scale(f, 0.5f)), tail = Vector3Subtract(c, Vector3Scale(f, 0.6f));
        Color k = { 8, 8, 10, 255 };
        DrawTriangle3D(nose, lt, tail, k); DrawTriangle3D(nose, tail, lt, k);
        DrawTriangle3D(nose, tail, rt, k); DrawTriangle3D(nose, rt, tail, k);
    }
}

/* Mist banks drifting around the cliff's foot. */
static void DrawMist(Camera3D cam, float time)
{
    int i;
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (i = 0; i < 10; i++) {
        float a = i * 0.63f + time * 0.02f * (i & 1 ? 1.0f : -1.0f);
        Vector3 p = { cosf(a) * (46.0f + 8.0f * Hash01(i, 1, 41)), 1.0f + 4.0f * Hash01(i, 2, 42), sinf(a) * (46.0f + 8.0f * Hash01(i, 1, 41)) };
        DrawBillboard(cam, cloudTex, p, 70.0f, (Color){ 120, 130, 150, 90 });
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

/* A jagged bolt from the clouds down behind the castle during the first instant of a strike. */
static void DrawBolt(float time)
{
    Vector3 p = { -40.0f + 30.0f * sinf(time), 140.0f, -90.0f };
    int k;
    if (lightningAge < 0.0f || lightningAge > 0.14f) return;
    for (k = 0; k < 9; k++) {
        Vector3 q = { p.x + (float)GetRandomValue(-60, 60) / 10.0f, p.y - 15.0f, p.z + (float)GetRandomValue(-30, 30) / 10.0f };
        DrawLine3D(p, q, (Color){ 230, 240, 255, 255 });
        DrawLine3D((Vector3){ p.x + 0.3f, p.y, p.z }, (Vector3){ q.x + 0.3f, q.y, q.z }, (Color){ 180, 200, 255, 255 });
        p = q;
    }
}

void Title_DrawScene(float time)
{
    Camera3D cam = TitleCamera(time);
    float flash = Flash(), wa = time * 0.11f, wb = -time * 0.09f + PI;
    int i;

    DrawRectangleGradientV(0, 0, POST_W, POST_H, (Color){ 14, 16, 32, 255 }, (Color){ 58, 64, 88, 255 });
    BeginMode3D(cam);
    DrawSky(cam, time, flash);

    /* light: two sweeping colour washes, the moon, and lightning from high above */
    Render_AddDynamicLight((Vector3){ cosf(wa) * 30.0f, CLIFF_H + 16.0f, sinf(wa) * 30.0f }, (Vector3){ 1.5f, 0.6f, 0.2f }, 44.0f);
    Render_AddDynamicLight((Vector3){ cosf(wb) * 30.0f, CLIFF_H + 22.0f, sinf(wb) * 30.0f }, (Vector3){ 0.22f, 0.95f, 1.05f }, 44.0f);
    Render_AddDynamicLight((Vector3){ -60.0f, 120.0f, 70.0f }, (Vector3){ 0.45f, 0.52f, 0.75f }, 200.0f);
    if (flash > 0.0f) Render_AddDynamicLight((Vector3){ -30.0f, 120.0f, 90.0f }, Vector3Scale((Vector3){ 0.9f, 0.95f, 1.2f }, 2.2f * flash), 230.0f);
    Render_SetLightning(flash);
    Render_SetFlare(0.0f);
    Render_BeginFrame(&TITLE_LOOK, NULL, cam, (Vector3){ 0.0f, -500.0f, 0.0f }, time);
    for (i = 0; i < partCount; i++) {
        int m = partMat[i];
        Material mat = Render_Material(m);
        bool glow = m == MAT_WINDOW_LIT || m == MAT_WINDOW_FLICKER || m == MAT_GLASS;
        if (m == MAT_WINDOW_FLICKER) {
            float f = 0.55f + 0.3f * sinf(time * 5.3f) + 0.15f * sinf(time * 13.7f);
            mat.maps[MATERIAL_MAP_DIFFUSE].color = (Color){ (unsigned char)(255 * f), (unsigned char)(255 * f), (unsigned char)(255 * f), 255 };
        }
        if (glow) Render_SetEmissive(true);
        DrawMesh(parts[i], mat, MatrixIdentity());
        if (glow) Render_SetEmissive(false);
    }
    DrawCrows(time);
    DrawBolt(time);
    DrawMist(cam, time);
    DrawSnow(cam, time);
    EndMode3D();
}

/* ============================================================ menus */

Rectangle Title_MenuItemRect(int index, int count)
{
    (void)count;
    return (Rectangle){ 70.0f, 330.0f + index * 58.0f, 360.0f, 50.0f };
}

static float Ease(float t) { t = Clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

/* A small flickering candle flame (hovered menu item). */
static void FlameIcon(float x, float y, float time)
{
    float f = 1.0f + 0.15f * sinf(time * 17.0f) + 0.1f * sinf(time * 29.0f);
    DrawTriangle((Vector2){ x, y - 14 * f }, (Vector2){ x - 6, y + 4 }, (Vector2){ x + 6, y + 4 }, (Color){ 255, 130, 30, 230 });
    DrawTriangle((Vector2){ x, y - 7 * f }, (Vector2){ x - 3, y + 3 }, (Vector2){ x + 3, y + 3 }, (Color){ 255, 236, 160, 255 });
    DrawRectangle((int)x - 2, (int)y + 4, 4, 10, (Color){ 220, 210, 180, 255 });
}

static void KeyCap(float x, float y, const char *key)
{
    Vector2 size = { fmaxf(44.0f, 16.0f + 11.0f * (float)strlen(key)), 34.0f };
    DrawRectangleRounded((Rectangle){ x, y + 3, size.x, size.y }, 0.25f, 4, (Color){ 10, 8, 6, 255 });
    DrawRectangleRounded((Rectangle){ x, y, size.x, size.y }, 0.25f, 4, (Color){ 58, 50, 42, 255 });
    DrawRectangleRoundedLinesEx((Rectangle){ x, y, size.x, size.y }, 0.25f, 4, 1.5f, (Color){ 150, 120, 70, 255 });
    UI_TextCentered(false, key, x + size.x * 0.5f, y + 5, 22, COL_BONE);
}

static void DrawControls(void)
{
    static const char *const keys[][2] = {
        { "W A S D", "Move" }, { "Mouse", "Look around" }, { "Left click", "Red Lightning" }, { "Shift", "Dash (dodge)" },
        { "E (hold)", "Open chest / talk" }, { "M", "Map" }, { "I / Tab", "Inventory" }, { "Esc / P", "Pause" },
        { "Enter", "Confirm" }, { "F1", "Debug overlay" }, { "F2", "PS1 effect on/off" }, { "F3", "God mode" },
        { "F4", "Finish this wing" }, { "F6", "Show FPS" },
    };
    int i, n = (int)(sizeof(keys) / sizeof(keys[0]));
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 4, 4, 8, 210 });
    UI_TextCentered(true, "Controls", SCREEN_W * 0.5f, 40, 70, COL_GOLD);
    for (i = 0; i < n; i++) {
        float x = i < 7 ? 170.0f : 690.0f, y = 160.0f + (i % 7) * 62.0f;
        KeyCap(x, y, keys[i][0]);
        UI_Text(false, keys[i][1], x + 170, y + 3, 28, COL_BONE);
    }
    UI_TextCentered(false, "Esc to go back", SCREEN_W * 0.5f, SCREEN_H - 50, 24, (Color){ 150, 140, 128, 255 });
}

static void DrawCredits(float t)
{
    static const char *const lines[] = {
        "#THE ASHEN MONASTERY", "", "Created by: ___", "", "", "#Fonts",
        "Pirata One  -  Rodrigo Fuenzalida, Nicolas Massi  (SIL Open Font License, Google Fonts)",
        "Crimson Text  -  Sebastian Kosch  (SIL Open Font License, Google Fonts)", "", "#Sounds",
        "RPG Audio and Impact Sounds  -  Kenney  (CC0)", "Ghost Moans  -  qubodup  (CC0, OpenGameArt)",
        "Scary High-pitched Ghost  -  Fupi  (CC0, OpenGameArt)", "Loopable Dungeon Ambience  -  JaggedStone  (CC0, OpenGameArt)",
        "", "#Textures  (Poly Haven, CC0)", "Stone Block Wall  -  Grey Plaster 02  -  Monastery Stone Floor",
        "Dark Wood  -  Plastered Stone Wall  -  Slate Floor  -  Rusty Metal 02  -  Dark Wooden Planks", "",
        "#Made in code", "Every character, relic, prop, the castle, the music of the bells, thunder and lightning,",
        "stained glass, banners and book spines - built with C and raylib", "", "#Library", "raylib 6.0  -  Ramon Santamaria (zlib)",
        "", "", "Thank you for playing.",
    };
    int i, n = (int)(sizeof(lines) / sizeof(lines[0]));
    float y = SCREEN_H + 20.0f - t * 42.0f;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 4, 4, 8, 215 });
    for (i = 0; i < n; i++, y += 40.0f) {
        bool head = lines[i][0] == '#';
        if (y < -60.0f || y > SCREEN_H + 10.0f) continue;
        UI_TextCentered(head, head ? lines[i] + 1 : lines[i], SCREEN_W * 0.5f, y, head ? 40.0f : 26.0f, head ? COL_GOLD : COL_BONE);
    }
    UI_TextCentered(false, "Esc to go back", SCREEN_W * 0.5f, SCREEN_H - 40, 22, (Color){ 150, 140, 128, 255 });
}

void Title_DrawUI(const Game *g)
{
    int actions[6], count, i;
    const char *labels[6];
    float t = g->titleTime, letters = (t - 1.6f) / 0.09f;
    float sub = Clamp((t - 1.6f - 19 * 0.09f - 0.3f) / 1.0f, 0.0f, 1.0f);

    if (g->menuPhase == 2) { DrawControls(); return; }
    if (g->menuPhase == 3) { DrawCredits(g->menuTime); return; }

    /* the title, letter by letter, with a soft glow; then the subtitle */
    if (g->menuPhase == 0) {
        UI_TextReveal("THE ASHEN MONASTERY", SCREEN_W * 0.5f, 150, 104, letters, (Color){ 214, 188, 140, 255 });
        UI_TextCentered(false, "Climb. Break the bells. Bring them home.", SCREEN_W * 0.5f, 278, 32, Fade(COL_BONE, sub));
        if (sub >= 1.0f) {
            float pulse = 0.45f + 0.55f * (0.5f + 0.5f * sinf(t * 3.0f));
            UI_TextCentered(false, "Press any key", SCREEN_W * 0.5f, 560, 34, Fade(COL_GOLD, pulse));
        }
    } else {
        float slide = Ease(g->menuTime / 0.5f);
        float dx = -460.0f * (1.0f - slide);
        DrawRectangleGradientH((int)dx, 0, 520, SCREEN_H, (Color){ 0, 0, 0, 215 }, (Color){ 0, 0, 0, 0 });
        UI_TextReveal("THE ASHEN MONASTERY", 250 + dx, 120, 64, 100.0f, (Color){ 214, 188, 140, 255 });
        UI_Text(false, "Climb. Break the bells. Bring them home.", 72 + dx, 200, 24, COL_BONE);
        count = Game_MenuOptions(g, actions, labels);
        for (i = 0; i < count; i++) {
            Rectangle r = Title_MenuItemRect(i, count);
            bool sel = i == g->menuSel;
            float size = sel ? 40.0f : 34.0f;
            if (sel) {
                DrawRectangleGradientH((int)(r.x + dx - 10), (int)r.y, (int)r.width, (int)r.height, (Color){ 120, 70, 20, 90 }, (Color){ 0, 0, 0, 0 });
                FlameIcon(r.x + dx + 4, r.y + 24, g->time);
            }
            UI_Text(false, labels[i], r.x + dx + 26, r.y + (sel ? 2 : 6), size, sel ? COL_GOLD : (Color){ 190, 180, 165, 255 });
        }
        UI_Text(false, "W/S or mouse to choose, Enter or click to select", 72 + dx, SCREEN_H - 50, 20, (Color){ 140, 130, 118, 255 });
    }
    /* black at launch, fading the scene in */
    if (t < 1.5f) DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 1.0f - t / 1.5f));
}
