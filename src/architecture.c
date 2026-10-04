/* architecture.c - turns a wing's grid into low-poly gothic architecture (CLAUDE.md 7.3):
 *   - rooms are 6.0 tall, corridors 3.5; where a corridor opens into a room the room wall
 *     continues above the opening (lintel) and a pointed arch frames the opening;
 *   - walls get a stone plinth and a cornice, long room walls get half-column pilasters,
 *     'P' cells become 8-sided columns, outer wall corners get a thin trim post;
 *   - small rooms get wooden ceiling beams, rooms of 8x8 and more get ribbed groin vaults,
 *     corridors get a flat ceiling with a beam every 3 units;
 *   - floors are flagstones, '=' crimson carpet runners with a gold edge, ',' floorboards.
 * Output: one Mesh per (16x16-cell chunk, material). Every face uses world-space UVs and big
 * faces are split into ~1x1 pieces (for the PS1 affine texturing and vertex wobble).
 * Light from the wall sconces is baked into the vertex colours. */
#include <string.h>
#include <math.h>
#include "geo.h"
#include "raymath.h"

static const int DX[4] = { 1, -1, 0, 0 };
static const int DZ[4] = { 0, 0, 1, -1 };

/* ============================================================ geometry builder */

static bool InBounds(const World *w, int x, int z) { return x >= 0 && z >= 0 && x < w->w && z < w->h; }
static bool IsOpen(const World *w, int x, int z) { return InBounds(w, x, z) && w->area[z][x] != AREA_SOLID; }

/* Deterministic pseudo-random 0..1 (decorations look the same every run). */
static float Hash01(int x, int y, int seed)
{
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ (unsigned int)seed * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
}

/* Vertex light. The world shader lights everything per pixel, so by default no light is baked
 * (BAKE_LIGHT 0); baking (cached by position) is kept for a shader-less fallback look. */
#define BAKE_LIGHT 0
#define LIGHT_CACHE 65536
static struct { int key[3]; Color c; bool used; } lightCache[LIGHT_CACHE];

static Color LightAt(const World *w, Vector3 p)
{
    if (!BAKE_LIGHT) { (void)w; (void)p; return (Color){ 130, 130, 135, 255 }; }
    {
    int k[3] = { (int)floorf(p.x * 50.0f + 0.5f), (int)floorf(p.y * 50.0f + 0.5f), (int)floorf(p.z * 50.0f + 0.5f) };
    unsigned int h = ((unsigned int)k[0] * 73856093u ^ (unsigned int)k[1] * 19349663u ^ (unsigned int)k[2] * 83492791u) % LIGHT_CACHE;
    Vector3 l;
    int probe;
    for (probe = 0; probe < 16; probe++) {
        unsigned int i = (h + (unsigned int)probe) % LIGHT_CACHE;
        if (!lightCache[i].used) {
            l = World_LightAt(w, p);
            lightCache[i].used = true;
            memcpy(lightCache[i].key, k, sizeof(k));
            lightCache[i].c = (Color){ (unsigned char)(l.x * 255.0f), (unsigned char)(l.y * 255.0f), (unsigned char)(l.z * 255.0f), 255 };
            return lightCache[i].c;
        }
        if (memcmp(lightCache[i].key, k, sizeof(k)) == 0) return lightCache[i].c;
    }
    l = World_LightAt(w, p);
    return (Color){ (unsigned char)(l.x * 255.0f), (unsigned char)(l.y * 255.0f), (unsigned char)(l.z * 255.0f), 255 };
    }
}

/* A quad a,b,c,d (counter-clockwise seen from the front) with world-space UVs. */
static void Quad(Geo *g, int mat, Vector3 a, Vector3 b, Vector3 c, Vector3 d)
{
    Vector3 p[4] = { a, b, c, d };
    Vector3 n = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a));
    Vector2 uv[4];
    Color l[4];
    int k;
    if (Vector3Length(n) < 1e-6f) n = Vector3CrossProduct(Vector3Subtract(c, b), Vector3Subtract(a, b));
    n = Vector3Normalize(n);
    for (k = 0; k < 4; k++) {
        uv[k] = MB_WorldUV(p[k], n);
        l[k] = LightAt(g->w, Vector3Add(p[k], Vector3Scale(n, 0.05f)));
    }
    MB_Quad(&g->mb[mat], p, uv, l);
}

/* The same quad seen from both sides. */
static void Quad2(Geo *g, int mat, Vector3 a, Vector3 b, Vector3 c, Vector3 d)
{
    Quad(g, mat, a, b, c, d);
    Quad(g, mat, d, c, b, a);
}

/* A rectangle from corner o along edges a and b, split into pieces of about 1 unit. */
static void Rect(Geo *g, int mat, Vector3 o, Vector3 a, Vector3 b)
{
    int na = (int)ceilf(Vector3Length(a) - 0.01f), nb = (int)ceilf(Vector3Length(b) - 0.01f), i, j;
    Vector3 pa, pb;
    if (na < 1) na = 1;
    if (nb < 1) nb = 1;
    pa = Vector3Scale(a, 1.0f / na);
    pb = Vector3Scale(b, 1.0f / nb);
    for (j = 0; j < nb; j++) {
        for (i = 0; i < na; i++) {
            Vector3 p0 = Vector3Add(o, Vector3Add(Vector3Scale(pa, (float)i), Vector3Scale(pb, (float)j)));
            Quad(g, mat, p0, Vector3Add(p0, pa), Vector3Add(Vector3Add(p0, pa), pb), Vector3Add(p0, pb));
        }
    }
}

/* An axis-aligned box (all six faces). */
static void Box(Geo *g, int mat, Vector3 mn, Vector3 mx)
{
    Vector3 d = Vector3Subtract(mx, mn);
    Rect(g, mat, (Vector3){ mn.x, mx.y, mx.z }, (Vector3){ d.x, 0, 0 }, (Vector3){ 0, 0, -d.z });   /* top */
    Rect(g, mat, (Vector3){ mn.x, mn.y, mn.z }, (Vector3){ d.x, 0, 0 }, (Vector3){ 0, 0, d.z });    /* bottom */
    Rect(g, mat, (Vector3){ mx.x, mn.y, mx.z }, (Vector3){ 0, 0, -d.z }, (Vector3){ 0, d.y, 0 });   /* +X */
    Rect(g, mat, (Vector3){ mn.x, mn.y, mn.z }, (Vector3){ 0, 0, d.z }, (Vector3){ 0, d.y, 0 });    /* -X */
    Rect(g, mat, (Vector3){ mn.x, mn.y, mx.z }, (Vector3){ d.x, 0, 0 }, (Vector3){ 0, d.y, 0 });    /* +Z */
    Rect(g, mat, (Vector3){ mx.x, mn.y, mn.z }, (Vector3){ -d.x, 0, 0 }, (Vector3){ 0, d.y, 0 });   /* -Z */
}

/* A box given by a center on the floor plan, its half sizes and a height range. */
static void BoxAt(Geo *g, int mat, float cx, float cz, float hx, float hz, float y0, float y1)
{
    Box(g, mat, (Vector3){ cx - hx, y0, cz - hz }, (Vector3){ cx + hx, y1, cz + hz });
}

/* A vertical prism with `sides` sides (8 = octagon) around (cx, cz). `from`..`to` are angles in
 * radians, so a half column against a wall uses half the circle. */
static void Prism(Geo *g, int mat, float cx, float cz, float r, float y0, float y1, int sides, float from, float to)
{
    int i, rows = (int)ceilf(y1 - y0 - 0.01f), j;
    if (rows < 1) rows = 1;
    for (i = 0; i < sides; i++) {
        float a0 = from + (to - from) * i / sides, a1 = from + (to - from) * (i + 1) / sides;
        Vector3 p0 = { cx + cosf(a0) * r, 0, cz + sinf(a0) * r }, p1 = { cx + cosf(a1) * r, 0, cz + sinf(a1) * r };
        for (j = 0; j < rows; j++) {
            float ya = y0 + (y1 - y0) * j / rows, yb = y0 + (y1 - y0) * (j + 1) / rows;
            Quad(g, mat, (Vector3){ p1.x, ya, p1.z }, (Vector3){ p0.x, ya, p0.z }, (Vector3){ p0.x, yb, p0.z }, (Vector3){ p1.x, yb, p1.z });
        }
    }
}

/* ---- exported builder API (geo.h) ---- */

void Geo_Quad(Geo *g, int mat, Vector3 a, Vector3 b, Vector3 c, Vector3 d) { Quad(g, mat, a, b, c, d); }
void Geo_Quad2(Geo *g, int mat, Vector3 a, Vector3 b, Vector3 c, Vector3 d) { Quad2(g, mat, a, b, c, d); }
void Geo_Box(Geo *g, int mat, Vector3 mn, Vector3 mx) { Box(g, mat, mn, mx); }

void Geo_QuadUV(Geo *g, int mat, const Vector3 p[4], const Vector2 uv[4], bool twoSided)
{
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(p[1], p[0]), Vector3Subtract(p[3], p[0])));
    Color l[4];
    int k;
    for (k = 0; k < 4; k++) l[k] = LightAt(g->w, Vector3Add(p[k], Vector3Scale(n, 0.05f)));
    MB_Quad(&g->mb[mat], p, uv, l);
    if (twoSided) {
        Vector3 q[4] = { p[3], p[2], p[1], p[0] };
        Vector2 v[4] = { uv[3], uv[2], uv[1], uv[0] };
        Color m[4] = { l[3], l[2], l[1], l[0] };
        MB_Quad(&g->mb[mat], q, v, m);
    }
}

Vector3 Frame_Point(Frame f, float x, float y, float z)
{
    float c = cosf(f.yaw), s = sinf(f.yaw);
    return (Vector3){ f.o.x + x * c + z * s, f.o.y + y, f.o.z - x * s + z * c };
}

void Geo_OBox(Geo *g, int mat, Vector3 center, float yaw, Vector3 h)
{
    Frame f = { center, yaw };
    Vector3 p[8];
    int i;
    for (i = 0; i < 8; i++)
        p[i] = Frame_Point(f, (i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
    /* corners: bit0 = +x, bit1 = +y, bit2 = +z */
    Quad(g, mat, p[2], p[6], p[7], p[3]);    /* top    (+y) */
    Quad(g, mat, p[0], p[1], p[5], p[4]);    /* bottom (-y) */
    Quad(g, mat, p[1], p[3], p[7], p[5]);    /* +x */
    Quad(g, mat, p[4], p[6], p[2], p[0]);    /* -x */
    Quad(g, mat, p[5], p[7], p[6], p[4]);    /* +z */
    Quad(g, mat, p[0], p[2], p[3], p[1]);    /* -z */
}

void Geo_FBox(Geo *g, int mat, Frame f, float x, float y, float z, float hx, float hy, float hz)
{
    Geo_OBox(g, mat, Frame_Point(f, x, y, z), f.yaw, (Vector3){ hx, hy, hz });
}

void Geo_Lathe(Geo *g, int mat, Vector3 base, const float *r, const float *y, int rings, int sides, bool capTop)
{
    int k, i;
    for (k = 0; k + 1 < rings; k++) {
        for (i = 0; i < sides; i++) {
            float a0 = 2.0f * PI * i / sides, a1 = 2.0f * PI * (i + 1) / sides;
            Vector3 p00 = { base.x + cosf(a0) * r[k], base.y + y[k], base.z + sinf(a0) * r[k] };
            Vector3 p01 = { base.x + cosf(a1) * r[k], base.y + y[k], base.z + sinf(a1) * r[k] };
            Vector3 p10 = { base.x + cosf(a0) * r[k + 1], base.y + y[k + 1], base.z + sinf(a0) * r[k + 1] };
            Vector3 p11 = { base.x + cosf(a1) * r[k + 1], base.y + y[k + 1], base.z + sinf(a1) * r[k + 1] };
            Quad(g, mat, p01, p00, p10, p11);
        }
    }
    if (capTop && rings > 0) {
        Vector3 c = { base.x, base.y + y[rings - 1], base.z };
        for (i = 0; i < sides; i++) {
            float a0 = 2.0f * PI * i / sides, a1 = 2.0f * PI * (i + 1) / sides;
            Vector3 p0 = { base.x + cosf(a0) * r[rings - 1], c.y, base.z + sinf(a0) * r[rings - 1] };
            Vector3 p1 = { base.x + cosf(a1) * r[rings - 1], c.y, base.z + sinf(a1) * r[rings - 1] };
            Quad(g, mat, p1, p0, c, c);
        }
    }
}

/* ============================================================ heights */

static float Profile(float t);

/* Ceiling height of an open cell at world point (px, pz): corridors are flat, small rooms are
 * flat, big rooms have a groin vault per bay (about 4x4 cells). */
static float CeilAt(const World *w, int x, int z, float px, float pz)
{
    const Room *r;
    float bw, bd, u, v;
    int nbx, nbz;
    if (!IsOpen(w, x, z)) return 0.0f;
    if (w->area[z][x] != AREA_ROOM) return CORRIDOR_HEIGHT;
    r = &w->rooms[w->roomId[z][x] - 1];
    bw = (float)(r->x1 - r->x0 + 1);
    bd = (float)(r->z1 - r->z0 + 1);
    if (bw < VAULT_MIN_ROOM || bd < VAULT_MIN_ROOM) return ROOM_HEIGHT;
    nbx = (int)(bw / 4.0f + 0.5f);
    nbz = (int)(bd / 4.0f + 0.5f);
    u = (px - r->x0) / bw * nbx;
    v = (pz - r->z0) / bd * nbz;
    u = Clamp(u - floorf(u), 0.0f, 1.0f);
    v = Clamp(v - floorf(v), 0.0f, 1.0f);
    if (px >= r->x1 + 1.0f - 0.001f) u = 1.0f;           /* far edges of the last bay */
    if (pz >= r->z1 + 1.0f - 0.001f) v = 1.0f;
    return VAULT_SPRING + (ROOM_HEIGHT - VAULT_SPRING) * fmaxf(Profile(u), Profile(v));
}

static bool IsVaulted(const World *w, int x, int z)
{
    const Room *r;
    if (!IsOpen(w, x, z) || w->area[z][x] != AREA_ROOM) return false;
    r = &w->rooms[w->roomId[z][x] - 1];
    return r->x1 - r->x0 + 1 >= VAULT_MIN_ROOM && r->z1 - r->z0 + 1 >= VAULT_MIN_ROOM;
}

/* Lowest ceiling over a cell (for the camera). */
static float CellCeiling(const World *w, int x, int z)
{
    float lo = 1e9f;
    int i, j;
    for (j = 0; j <= 2; j++)
        for (i = 0; i <= 2; i++) lo = fminf(lo, CeilAt(w, x, z, x + i * 0.5f, z + j * 0.5f));
    return lo;
}

/* Pointed (two-centred) arch profile: t 0..1 across the span -> height 0..1. Each half is a
 * circle arc of radius 0.75 x span centred on the far springing side. */
static float Profile(float t)
{
    const float r = 0.75f, apex = 0.70710678f;            /* sqrt(r^2 - (0.5 - r)^2) */
    float d;
    if (t > 0.5f) t = 1.0f - t;
    if (t < 0.0f) t = 0.0f;
    d = t - r;
    return sqrtf(fmaxf(0.0f, r * r - d * d)) / apex;
}

/* ============================================================ walls */

/* Wall material of a solid cell. */
static int WallMat(char c)
{
    if (c == 'W') return MAT_WOOD_PANEL;
    return MAT_WALL_STONE;
}

/* A vertical panel from floor point A to B (front faces the direction cross(up, B - A)... i.e.
 * the side you see when A is on your left), between height y0 and a top that follows tops at
 * A and B. Split into `cols` columns and rows of about 1 unit. */
static void Panel(Geo *g, int mat, Vector2 A, Vector2 B, float y0, float topA, float topB, int cols)
{
    int i, j, rows = (int)ceilf(fmaxf(topA, topB) - y0 - 0.01f);
    if (rows < 1) rows = 1;
    for (i = 0; i < cols; i++) {
        float t0 = (float)i / cols, t1 = (float)(i + 1) / cols;
        Vector2 a = Vector2Lerp(A, B, t0), b = Vector2Lerp(A, B, t1);
        float ta = topA + (topB - topA) * t0, tb = topA + (topB - topA) * t1;
        if (ta <= y0 + 0.001f && tb <= y0 + 0.001f) continue;
        for (j = 0; j < rows; j++) {
            float ya0 = y0 + (ta - y0) * j / rows, ya1 = y0 + (ta - y0) * (j + 1) / rows;
            float yb0 = y0 + (tb - y0) * j / rows, yb1 = y0 + (tb - y0) * (j + 1) / rows;
            Quad(g, mat, (Vector3){ a.x, ya0, a.y }, (Vector3){ b.x, yb0, b.y }, (Vector3){ b.x, yb1, b.y }, (Vector3){ a.x, ya1, a.y });
        }
    }
}

/* Edge of open cell (x,z) toward direction d: endpoints A,B with the wall seen from the cell. */
static void EdgeOf(int x, int z, int d, Vector2 *A, Vector2 *B, Vector2 *n)
{
    Vector2 e = { x + 0.5f + DX[d] * 0.5f, z + 0.5f + DZ[d] * 0.5f };
    Vector2 t;
    *n = (Vector2){ (float)-DX[d], (float)-DZ[d] };           /* points back into the cell */
    t = (Vector2){ n->y, -n->x };                              /* A -> B so the front faces the cell */
    *A = Vector2Subtract(e, Vector2Scale(t, 0.5f));
    *B = Vector2Add(e, Vector2Scale(t, 0.5f));
}

/* Box hugging a wall edge: from A to B, `depth` out from the wall, between y0 and y1. */
static void EdgeBox(Geo *g, int mat, Vector2 A, Vector2 B, Vector2 n, float depth, float y0, float y1)
{
    Vector2 o = Vector2Add(A, Vector2Scale(n, depth)), p = Vector2Add(B, Vector2Scale(n, depth));
    Box(g, mat, (Vector3){ fminf(fminf(A.x, B.x), fminf(o.x, p.x)), y0, fminf(fminf(A.y, B.y), fminf(o.y, p.y)) },
                (Vector3){ fmaxf(fmaxf(A.x, B.x), fmaxf(o.x, p.x)), y1, fmaxf(fmaxf(A.y, B.y), fmaxf(o.y, p.y)) });
}

/* Height of the string course (cornice) on the walls of cell (x,z). */
static float CorniceTop(const World *w, int x, int z)
{
    if (w->area[z][x] != AREA_ROOM) return CORRIDOR_HEIGHT;
    return IsVaulted(w, x, z) ? VAULT_SPRING : ROOM_HEIGHT;
}

/* ============================================================ windows */

/* Is the wall cell (wx, wz), seen from the inside in direction d, an outside wall? (Only solid
 * cells between it and the map border, so the moon can shine in.) */
static bool IsExterior(const World *w, int wx, int wz, int d)
{
    int x = wx, z = wz;
    while (InBounds(w, x, z)) {
        if (IsOpen(w, x, z)) return false;
        x += DX[d];
        z += DZ[d];
    }
    return true;
}

static bool WindowWall(const World *w, int x, int z, int d)
{
    int wx = x + DX[d], wz = z + DZ[d];
    char c;
    if (!IsOpen(w, x, z) || w->grid[z][x] == 'E' || IsOpen(w, wx, wz) || !InBounds(w, wx, wz)) return false;
    c = w->grid[wz][wx];
    return (c == '#' || c == 'W') && !w->island[wz][wx] && IsExterior(w, wx, wz, d);
}

/* Window heights for a cell: sill, where the lancets start curving, apex. */
static void WindowSize(const World *w, int x, int z, float *sill, float *spring, float *apex)
{
    if (w->area[z][x] == AREA_ROOM) { *sill = 1.4f; *spring = 3.1f; *apex = 3.82f; }
    else                            { *sill = 1.0f; *spring = 2.3f; *apex = 2.95f; }
}

/* Pointed-arch windows every ~5 cells along outside walls (rooms and corridors), each a cold
 * blue light source. Marked before the props are placed so nothing stands in front of them. */
static void FindWindows(World *w)
{
    int x, z, d;
    memset(w->window, 0, sizeof(w->window));
    w->windowCount = 0;
    for (d = 0; d < 4; d++) {
        for (z = 0; z < w->h; z++) {
            for (x = 0; x < w->w; x++) {
                bool alongX = DZ[d] != 0;
                int along = alongX ? x : z, k, run = 0;
                float sill, spring, apex;
                Vector2 A, B, n;
                Vector3 c;
                if (!WindowWall(w, x, z, d)) continue;
                /* position within the straight run of window-able wall: skip the ends */
                for (k = 1; k < 3 && WindowWall(w, x - (alongX ? k : 0), z - (alongX ? 0 : k), d); k++) run++;
                if (run < 1 || !WindowWall(w, x + (alongX ? 1 : 0), z + (alongX ? 0 : 1), d)) continue;
                if ((along + (int)(Hash01(d, alongX ? z : x, 5) * 3.0f)) % WINDOW_SPACING != 0) continue;
                if (w->windowCount >= MAX_WINDOWS) continue;
                w->window[z + DZ[d]][x + DX[d]] |= (unsigned char)(1 << d);
                WindowSize(w, x, z, &sill, &spring, &apex);
                EdgeOf(x, z, d, &A, &B, &n);
                c = (Vector3){ (A.x + B.x) * 0.5f, (sill + apex) * 0.5f, (A.y + B.y) * 0.5f };
                w->windows[w->windowCount].center = (Vector3){ c.x + n.x * 0.03f, c.y, c.z + n.y * 0.03f };
                w->windows[w->windowCount].normal = (Vector3){ n.x, 0.0f, n.y };
                w->windows[w->windowCount].side = Vector3Normalize((Vector3){ B.x - A.x, 0.0f, B.y - A.y });
                w->windows[w->windowCount].halfWidth = 0.38f;
                w->windows[w->windowCount].sill = sill;
                w->windows[w->windowCount].top = apex;
                w->windowCount++;
                World_AddLight(w, (Vector3){ c.x + n.x * 1.2f, c.y - 0.3f, c.z + n.y * 1.2f },
                               (Vector3){ MOON_R, MOON_G, MOON_B }, WINDOW_LIGHT_RADIUS);
                if (w->torchCount > 0) w->torches[w->torchCount - 1].flicker = 0.0f;     /* moonlight is steady */
            }
        }
    }
}

/* One lancet of glass: columns from x0 to x1 (wall-local), pointed top, UVs over the whole window. */
static void Lancet(Geo *g, Frame f, float x0, float x1, float sill, float spring, float apex, float wx0, float wx1)
{
    const int seg = 4;
    int i;
    for (i = 0; i < seg; i++) {
        float a = x0 + (x1 - x0) * i / seg, b = x0 + (x1 - x0) * (i + 1) / seg;
        float ta = spring + (apex - spring) * Profile((a - x0) / (x1 - x0));
        float tb = spring + (apex - spring) * Profile((b - x0) / (x1 - x0));
        Vector3 p[4] = { Frame_Point(f, a, sill, 0.03f), Frame_Point(f, b, sill, 0.03f),
                         Frame_Point(f, b, tb, 0.03f), Frame_Point(f, a, ta, 0.03f) };
        float ua = (a - wx0) / (wx1 - wx0), ub = (b - wx0) / (wx1 - wx0);
        Vector2 uv[4] = { { ua, 1.0f }, { ub, 1.0f }, { ub, 1.0f - (tb - sill) / (apex - sill) }, { ua, 1.0f - (ta - sill) / (apex - sill) } };
        Geo_QuadUV(g, MAT_GLASS, p, uv, false);
    }
}

/* A pointed-arch window with two lancets, a mullion, a stone sill, a trim frame and a small
 * diamond of glass in the tracery above the lancets. */
static void BuildWindow(Geo *g, int x, int z, int d)
{
    const World *w = g->w;
    const float hw = 0.40f, band = 0.09f, seg = 2 * ARCH_SEGMENTS;
    float sill, spring, apex, ls, la;
    Vector2 A, B, n;
    Frame f;
    int i;
    WindowSize(w, x, z, &sill, &spring, &apex);
    EdgeOf(x, z, d, &A, &B, &n);
    f.o = (Vector3){ (A.x + B.x) * 0.5f, 0.0f, (A.y + B.y) * 0.5f };
    f.yaw = atan2f(n.x, n.y);
    ls = spring;                                   /* lancets: lower apex than the outer arch */
    la = spring + (apex - spring) * 0.55f;
    Lancet(g, f, -0.36f, -0.03f, sill, ls, la, -0.36f, 0.36f);
    Lancet(g, f, 0.03f, 0.36f, sill, ls, la, -0.36f, 0.36f);
    {
        /* diamond light in the tracery */
        float cy = (la + apex) * 0.5f + 0.02f, r = (apex - la) * 0.32f;
        Vector3 p[4] = { Frame_Point(f, 0, cy - r, 0.03f), Frame_Point(f, r * 0.8f, cy, 0.03f),
                         Frame_Point(f, 0, cy + r, 0.03f), Frame_Point(f, -r * 0.8f, cy, 0.03f) };
        const Vector2 uv[4] = { { 0.5f, 0.35f }, { 0.6f, 0.2f }, { 0.5f, 0.05f }, { 0.4f, 0.2f } };
        Geo_QuadUV(g, MAT_GLASS, p, uv, false);
    }
    Geo_FBox(g, MAT_TRIM, f, 0, (sill + la) * 0.5f, 0.05f, 0.03f, (la - sill) * 0.5f, 0.05f);    /* mullion */
    Geo_FBox(g, MAT_TRIM, f, 0, sill - 0.06f, 0.1f, hw + 0.08f, 0.06f, 0.12f);                   /* sill */
    Geo_FBox(g, MAT_TRIM, f, -hw - band * 0.5f, (sill + spring) * 0.5f, 0.05f, band * 0.5f, (spring - sill) * 0.5f, 0.05f);  /* jambs */
    Geo_FBox(g, MAT_TRIM, f, hw + band * 0.5f, (sill + spring) * 0.5f, 0.05f, band * 0.5f, (spring - sill) * 0.5f, 0.05f);
    for (i = 0; i < (int)seg; i++) {                /* the arch band around the top */
        float t0 = i / seg, t1 = (i + 1) / seg;
        float x0 = -hw + 2 * hw * t0, x1 = -hw + 2 * hw * t1;
        float y0 = spring + (apex - spring) * Profile(t0), y1 = spring + (apex - spring) * Profile(t1);
        float X0 = -(hw + band) + 2 * (hw + band) * t0, X1 = -(hw + band) + 2 * (hw + band) * t1;
        float Y0 = spring + (apex + band - spring) * Profile(t0), Y1 = spring + (apex + band - spring) * Profile(t1);
        Geo_Quad(g, MAT_TRIM, Frame_Point(f, x0, y0, 0.1f), Frame_Point(f, x1, y1, 0.1f), Frame_Point(f, X1, Y1, 0.1f), Frame_Point(f, X0, Y0, 0.1f));
        Geo_Quad(g, MAT_TRIM, Frame_Point(f, x1, y1, 0.0f), Frame_Point(f, x1, y1, 0.1f), Frame_Point(f, x0, y0, 0.1f), Frame_Point(f, x0, y0, 0.0f));
    }
}

/* The wall of open cell (x,z) toward solid neighbour direction d: main face, plinth, cornice;
 * bookshelf walls get shelves of books up to BOOKSHELF_HEIGHT. */
static void WallEdge(Geo *g, int x, int z, int d)
{
    const World *w = g->w;
    int nx = x + DX[d], nz = z + DZ[d];
    char c = InBounds(w, nx, nz) ? w->grid[nz][nx] : '#';
    Vector2 A, B, n;
    float topA, topB, corn = CorniceTop(w, x, z);
    EdgeOf(x, z, d, &A, &B, &n);
    topA = CeilAt(w, x, z, A.x, A.y);
    topB = CeilAt(w, x, z, B.x, B.y);

    if (c == 'B' && w->island[nz][nx]) {
        /* free-standing bookcase: books up to the top board, nothing above */
        Vector2 a2 = Vector2Add(A, Vector2Scale(n, 0.02f)), b2 = Vector2Add(B, Vector2Scale(n, 0.02f));
        Panel(g, MAT_BOOKS, a2, b2, 0.0f, BOOKSHELF_HEIGHT, BOOKSHELF_HEIGHT, 1);
        Panel(g, MAT_WOOD, A, B, BOOKSHELF_HEIGHT, BOOKSHELF_HEIGHT + 0.12f, BOOKSHELF_HEIGHT + 0.12f, 1);
        EdgeBox(g, MAT_WOOD, A, B, n, 0.10f, 0.0f, 0.12f);
        return;
    }
    if (c == 'B') {
        /* bookshelf wall: wooden case with book rows, stone above */
        Vector2 a2 = Vector2Add(A, Vector2Scale(n, 0.02f)), b2 = Vector2Add(B, Vector2Scale(n, 0.02f));
        Panel(g, MAT_BOOKS, a2, b2, 0.0f, BOOKSHELF_HEIGHT, BOOKSHELF_HEIGHT, 1);
        Panel(g, MAT_WALL_STONE, A, B, BOOKSHELF_HEIGHT, topA, topB, 2);
        EdgeBox(g, MAT_WOOD, A, B, n, 0.14f, BOOKSHELF_HEIGHT, BOOKSHELF_HEIGHT + 0.12f);     /* top board */
        EdgeBox(g, MAT_WOOD, A, B, n, 0.10f, 0.0f, 0.12f);                                     /* kick board */
    } else {
        Panel(g, WallMat(c), A, B, 0.0f, topA, topB, 2);
        EdgeBox(g, MAT_TRIM, A, B, n, PLINTH_DEPTH, 0.0f, PLINTH_HEIGHT);
    }
    EdgeBox(g, MAT_TRIM, A, B, n, CORNICE_DEPTH, corn - CORNICE_HEIGHT, corn);
    if (w->window[nz][nx] & (1 << d)) BuildWindow(g, x, z, d);
}

/* Where a lower open cell (corridor) meets a higher one (room): the room wall continues above
 * the opening, from the corridor ceiling up to the room ceiling (seen from the room). */
static void LintelEdge(Geo *g, int x, int z, int d)
{
    const World *w = g->w;
    int nx = x + DX[d], nz = z + DZ[d];
    Vector2 A, B, n;
    float low = CeilAt(w, nx, nz, x + 0.5f, z + 0.5f);
    EdgeOf(x, z, d, &A, &B, &n);
    {
        int i;
        for (i = 0; i < 2; i++) {                            /* two halves follow the vault */
            Vector2 a = Vector2Lerp(A, B, i * 0.5f), b = Vector2Lerp(A, B, i * 0.5f + 0.5f);
            Panel(g, MAT_WALL_STONE, a, b, low, CeilAt(w, x, z, a.x, a.y), CeilAt(w, x, z, b.x, b.y), 1);
        }
    }
    EdgeBox(g, MAT_TRIM, A, B, n, CORNICE_DEPTH, CorniceTop(w, x, z) - CORNICE_HEIGHT, CorniceTop(w, x, z));
}

/* ============================================================ arches over openings */

typedef struct {
    Vector2 A, B, n;    /* opening from A to B (seen from the room side), n points into the room */
    float   top;        /* height of the opening (corridor ceiling) */
    float   spring;     /* where the arch curve starts */
    bool    pointed;    /* false: wide opening with a straight trimmed lintel */
} Opening;

static Vector3 ArchPoint(const Opening *o, float t, float widen, float raise, float out)
{
    Vector2 dir = Vector2Normalize(Vector2Subtract(o->B, o->A));
    Vector2 a = Vector2Subtract(o->A, Vector2Scale(dir, widen)), b = Vector2Add(o->B, Vector2Scale(dir, widen));
    Vector2 p = Vector2Add(Vector2Lerp(a, b, t), Vector2Scale(o->n, out));
    float h = o->pointed ? o->spring + (o->top + raise - o->spring) * Profile(t) : o->top + raise;
    return (Vector3){ p.x, h, p.y };
}

/* Pointed arch: stone infill between the curve and the opening's top (both sides), a trim band
 * following the curve on the room side, and trim jambs down to the floor. */
static void BuildArch(Geo *g, const Opening *o)
{
    const int seg = 2 * ARCH_SEGMENTS;
    const float band = 0.16f, out = 0.10f;
    Vector2 dir = Vector2Normalize(Vector2Subtract(o->B, o->A));
    int i;
    for (i = 0; i < seg; i++) {
        float t0 = (float)i / seg, t1 = (float)(i + 1) / seg;
        Vector3 c0 = ArchPoint(o, t0, 0, 0, 0), c1 = ArchPoint(o, t1, 0, 0, 0);
        Vector3 u0 = { c0.x, o->top, c0.z }, u1 = { c1.x, o->top, c1.z };
        Vector3 f0 = ArchPoint(o, t0, 0, 0, out), f1 = ArchPoint(o, t1, 0, 0, out);
        Vector3 b0 = ArchPoint(o, t0, band, band, out), b1 = ArchPoint(o, t1, band, band, out);
        if (o->pointed && o->top - c0.y + o->top - c1.y > 0.002f) Quad2(g, MAT_WALL_STONE, c1, c0, u0, u1);   /* infill */
        Quad(g, MAT_TRIM, f1, f0, b0, b1);                                                     /* band front */
        Quad(g, MAT_TRIM, c1, c0, f0, f1);                                                     /* band underside */
    }
    /* jambs: trim strips down both sides */
    {
        Vector2 a = o->A, b = o->B, an = Vector2Subtract(a, Vector2Scale(dir, band)), bn = Vector2Add(b, Vector2Scale(dir, band));
        float y = o->pointed ? o->spring : o->top;
        EdgeBox(g, MAT_TRIM, an, a, o->n, out, 0.0f, y);
        EdgeBox(g, MAT_TRIM, b, bn, o->n, out, 0.0f, y);
    }
}

/* Is the edge from open cell (x,z) toward d an arch opening? Room -> lower corridor, or any
 * cell -> exit doorway. */
static bool IsArchEdge(const World *w, int x, int z, int d)
{
    int nx = x + DX[d], nz = z + DZ[d];
    if (!IsOpen(w, x, z) || !IsOpen(w, nx, nz) || w->grid[z][x] == 'E') return false;
    if (w->grid[nz][nx] == 'E') return true;
    return w->area[z][x] == AREA_ROOM && w->area[nz][nx] == AREA_CORRIDOR;
}

static Opening MakeOpening(const World *w, int x0, int z0, int x1, int z1, int d)
{
    Opening o;
    Vector2 A, B, n, A2, B2;
    float width;
    EdgeOf(x0, z0, d, &A, &B, &n);
    EdgeOf(x1, z1, d, &A2, &B2, &n);
    o.A = Vector2Distance(A, B2) > Vector2Distance(A2, B) ? A : A2;     /* the outer endpoints */
    o.B = Vector2Distance(A, B2) > Vector2Distance(A2, B) ? B2 : B;
    o.n = n;
    o.top = CeilAt(w, x0 + DX[d], z0 + DZ[d], x0 + 0.5f, z0 + 0.5f);
    width = Vector2Distance(o.A, o.B);
    o.pointed = width <= ARCH_MAX_WIDTH + 0.01f;
    o.spring = fmaxf(ARCH_SPRING_MIN, o.top - 0.70710678f * width);
    return o;
}

/* Find runs of neighbouring arch edges along each wall line and build one arch per run.
 * Exit doorway openings are also recorded in the world (the door leaves are drawn there). */
static void BuildArches(Geo *g)
{
    World *w = g->w;
    static unsigned char done[WORLD_MAX_H][WORLD_MAX_W][4];
    int x, z, d;
    memset(done, 0, sizeof(done));
    w->doorwayCount = 0;
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            for (d = 0; d < 4; d++) {
                int sx = DZ[d] != 0 ? 1 : 0, sz = DX[d] != 0 ? 1 : 0;     /* step along the wall line */
                int ex = x, ez = z, len = 1;
                bool door;
                Opening o;
                if (done[z][x][d] || !IsArchEdge(w, x, z, d)) continue;
                door = w->grid[z + DZ[d]][x + DX[d]] == 'E';
                while (IsArchEdge(w, ex + sx, ez + sz, d) && !done[ez + sz][ex + sx][d] &&
                       (w->grid[ez + sz + DZ[d]][ex + sx + DX[d]] == 'E') == door) {
                    ex += sx; ez += sz; len++;
                    done[ez][ex][d] = 1;
                }
                done[z][x][d] = 1;
                o = MakeOpening(w, x, z, ex, ez, d);
                if (door) o.pointed = true;
                BuildArch(g, &o);
                if (door && w->doorwayCount < MAX_EXIT_CELLS) {
                    Doorway *dw = &w->doorways[w->doorwayCount++];
                    dw->a = (Vector3){ o.A.x, 0.0f, o.A.y };
                    dw->b = (Vector3){ o.B.x, 0.0f, o.B.y };
                    dw->inward = (Vector3){ o.n.x, 0.0f, o.n.y };
                    dw->spring = o.spring;
                    dw->top = o.top;
                }
                (void)len;
            }
        }
    }
}

/* ============================================================ ceilings */

/* Ceiling over one open cell: split into 2x2 pieces that follow the vault (or stay flat). */
static void CeilingCell(Geo *g, int x, int z)
{
    const World *w = g->w;
    int i, j;
    for (j = 0; j < 2; j++) {
        for (i = 0; i < 2; i++) {
            float x0 = x + i * 0.5f, x1 = x0 + 0.5f, z0 = z + j * 0.5f, z1 = z0 + 0.5f;
            Quad(g, MAT_CEILING, (Vector3){ x0, CeilAt(w, x, z, x0, z0), z0 }, (Vector3){ x1, CeilAt(w, x, z, x1, z0), z0 },
                 (Vector3){ x1, CeilAt(w, x, z, x1, z1), z1 }, (Vector3){ x0, CeilAt(w, x, z, x0, z1), z1 });
        }
    }
}

/* A rib: a trim strip hanging under the vault along a line from p to q (floor plan). */
static void Rib(Geo *g, int x, int z, Vector2 p, Vector2 q)
{
    const World *w = g->w;
    const int seg = 6;
    const float hw = 0.07f, depth = 0.14f;
    Vector2 dir = Vector2Normalize(Vector2Subtract(q, p)), side = { -dir.y * hw, dir.x * hw };
    int i;
    for (i = 0; i < seg; i++) {
        Vector2 a = Vector2Lerp(p, q, (float)i / seg), b = Vector2Lerp(p, q, (float)(i + 1) / seg);
        float ha = CeilAt(w, x, z, a.x, a.y) - 0.01f, hb = CeilAt(w, x, z, b.x, b.y) - 0.01f;
        Vector3 al = { a.x - side.x, ha, a.y - side.y }, ar = { a.x + side.x, ha, a.y + side.y };
        Vector3 bl = { b.x - side.x, hb, b.y - side.y }, br = { b.x + side.x, hb, b.y + side.y };
        Vector3 al2 = { al.x, ha - depth, al.z }, ar2 = { ar.x, ha - depth, ar.z };
        Vector3 bl2 = { bl.x, hb - depth, bl.z }, br2 = { br.x, hb - depth, br.z };
        Quad2(g, MAT_TRIM, al2, ar2, br2, bl2);          /* underside */
        Quad2(g, MAT_TRIM, al, al2, bl2, bl);            /* sides */
        Quad2(g, MAT_TRIM, ar2, ar, br, br2);
    }
}

/* Ribs of each ~4x4 bay of a vaulted room (room number id, 1-based): both diagonals + the
 * arches between bays. Bays whose centre is not in the room (odd-shaped rooms) are skipped. */
static void VaultRibs(Geo *g, const Room *r, int id)
{
    const World *w = g->w;
    float bw = (float)(r->x1 - r->x0 + 1), bd = (float)(r->z1 - r->z0 + 1);
    int nbx = (int)(bw / 4.0f + 0.5f), nbz = (int)(bd / 4.0f + 0.5f), i, j;
    float sx = bw / nbx, sz = bd / nbz;
    for (j = 0; j < nbz; j++) {
        for (i = 0; i < nbx; i++) {
            float x0 = r->x0 + i * sx, z0 = r->z0 + j * sz, x1 = x0 + sx, z1 = z0 + sz;
            int cx = (int)((x0 + x1) * 0.5f), cz = (int)((z0 + z1) * 0.5f);
            if (!IsOpen(w, cx, cz) || w->roomId[cz][cx] != id) continue;
            Rib(g, cx, cz, (Vector2){ x0 + 0.05f, z0 + 0.05f }, (Vector2){ x1 - 0.05f, z1 - 0.05f });
            Rib(g, cx, cz, (Vector2){ x1 - 0.05f, z0 + 0.05f }, (Vector2){ x0 + 0.05f, z1 - 0.05f });
            if (i > 0) Rib(g, cx, cz, (Vector2){ x0 + 0.08f, z0 + 0.05f }, (Vector2){ x0 + 0.08f, z1 - 0.05f });
            if (j > 0) Rib(g, cx, cz, (Vector2){ x0 + 0.05f, z0 + 0.08f }, (Vector2){ x1 - 0.05f, z0 + 0.08f });
        }
    }
}

/* Length of the straight run of open cells through (x,z) along x (axis 0) or z (axis 1). */
static int RunLength(const World *w, int x, int z, int axis)
{
    int n = 1, k;
    for (k = 1; k < 8 && IsOpen(w, x + (axis == 0 ? k : 0), z + (axis == 1 ? k : 0)); k++) n++;
    for (k = 1; k < 8 && IsOpen(w, x - (axis == 0 ? k : 0), z - (axis == 1 ? k : 0)); k++) n++;
    return n;
}

/* Dark wooden beams: flat rooms every 2 units across the short side, corridors every 3 units
 * across the corridor. */
static void Beams(Geo *g, int x, int z)
{
    const World *w = g->w;
    float top;
    if (IsVaulted(w, x, z)) return;
    top = CeilAt(w, x, z, x + 0.5f, z + 0.5f);
    if (w->area[z][x] == AREA_ROOM) {
        const Room *r = &w->rooms[w->roomId[z][x] - 1];
        bool alongZ = (r->x1 - r->x0) >= (r->z1 - r->z0);    /* beams span the short (z) side */
        int k = alongZ ? x - r->x0 : z - r->z0;
        if (k % 2 != 1) return;
        if (alongZ) BoxAt(g, MAT_WOOD, x + 1.0f, z + 0.5f, 0.12f, 0.5f, top - 0.3f, top);
        else        BoxAt(g, MAT_WOOD, x + 0.5f, z + 1.0f, 0.5f, 0.12f, top - 0.3f, top);
    } else {
        bool runX = RunLength(w, x, z, 0) >= RunLength(w, x, z, 1);   /* corridor runs along x */
        if (runX && x % 3 == 0) BoxAt(g, MAT_WOOD, x + 0.5f, z + 0.5f, 0.11f, 0.5f, top - 0.24f, top);
        if (!runX && z % 3 == 0) BoxAt(g, MAT_WOOD, x + 0.5f, z + 0.5f, 0.5f, 0.11f, top - 0.24f, top);
    }
}

/* ============================================================ details */

/* 'P': an 8-sided column with a square base and capital, up to the ceiling. */
static void Column(Geo *g, int x, int z)
{
    float cx = x + 0.5f, cz = z + 0.5f, top = CeilAt(g->w, x, z, cx, cz);
    BoxAt(g, MAT_TRIM, cx, cz, 0.42f, 0.42f, 0.0f, 0.30f);
    BoxAt(g, MAT_TRIM, cx, cz, 0.36f, 0.36f, 0.30f, 0.45f);
    Prism(g, MAT_PILLAR, cx, cz, 0.33f, 0.45f, top - 0.45f, 8, 0.0f, 2.0f * PI);
    BoxAt(g, MAT_TRIM, cx, cz, 0.36f, 0.36f, top - 0.45f, top - 0.30f);
    BoxAt(g, MAT_TRIM, cx, cz, 0.44f, 0.44f, top - 0.30f, top);
}

/* Half column against a wall at floor point p, facing n, up to `top`. */
static void Pilaster(Geo *g, Vector2 p, Vector2 n, float top)
{
    float a = atan2f(n.y, n.x);
    Vector2 c = Vector2Add(p, Vector2Scale(n, 0.02f));
    BoxAt(g, MAT_TRIM, c.x, c.y, fabsf(n.x) > 0.5f ? 0.18f : 0.30f, fabsf(n.x) > 0.5f ? 0.30f : 0.18f, 0.0f, PLINTH_HEIGHT + 0.12f);
    Prism(g, MAT_PILLAR, c.x, c.y, 0.2f, PLINTH_HEIGHT + 0.12f, top - 0.32f, 4, a - PI * 0.5f, a + PI * 0.5f);
    BoxAt(g, MAT_TRIM, c.x, c.y, fabsf(n.x) > 0.5f ? 0.22f : 0.32f, fabsf(n.x) > 0.5f ? 0.32f : 0.22f, top - 0.32f, top);
}

static bool IsPlainWall(const World *w, int x, int z)
{
    char c;
    if (!InBounds(w, x, z) || w->window[z][x]) return false;
    c = w->grid[z][x];
    return c == '#' || c == 'W';
}

/* Is the wall next to room cell (x,z) in direction d plain (room cell, plain wall)? */
static bool PlainRoomWall(const World *w, int x, int z, int d)
{
    return IsOpen(w, x, z) && w->area[z][x] == AREA_ROOM && IsPlainWall(w, x + DX[d], z + DZ[d]);
}

/* Pilasters along long room walls: every 4 cells, or in vaulted rooms where the bays meet (so
 * the ribs seem to rest on them). Never on sconces, bookshelves, windows, doors or openings. */
static void Pilasters(Geo *g, int x, int z, int d)
{
    const World *w = g->w;
    const Room *r;
    bool alongX = DZ[d] != 0;                     /* this wall runs along the x axis */
    int len, start, cell, k, nb;
    float line, bay;
    if (!PlainRoomWall(w, x, z, d)) return;
    r = &w->rooms[w->roomId[z][x] - 1];
    len = alongX ? r->x1 - r->x0 + 1 : r->z1 - r->z0 + 1;
    start = alongX ? r->x0 : r->z0;
    cell = alongX ? x : z;
    if (len < 6) return;
    nb = IsVaulted(w, x, z) ? (int)(len / 4.0f + 0.5f) : len / 4 + 1;
    bay = IsVaulted(w, x, z) ? (float)len / nb : 4.0f;
    for (k = 1; k < nb + 1; k++) {
        float off = IsVaulted(w, x, z) ? 0.0f : -2.0f;      /* flat rooms: 2, 6, 10, ... */
        line = start + k * bay + off;
        if (line <= start + 0.9f || line >= start + len - 0.9f) continue;
        if (line < cell || line >= cell + 1.0f) continue;
        if (line - cell < 0.3f && !PlainRoomWall(w, x - (alongX ? 1 : 0), z - (alongX ? 0 : 1), d)) continue;
        if (line - cell > 0.7f && !PlainRoomWall(w, x + (alongX ? 1 : 0), z + (alongX ? 0 : 1), d)) continue;
        {
            float wallLine = alongX ? z + 0.5f + DZ[d] * 0.5f : x + 0.5f + DX[d] * 0.5f;
            Vector2 p = alongX ? (Vector2){ line, wallLine } : (Vector2){ wallLine, line };
            Pilaster(g, p, (Vector2){ (float)-DX[d], (float)-DZ[d] }, CorniceTop(w, x, z) - CORNICE_HEIGHT);
        }
    }
}

/* Thin vertical trim on outer wall corners (a corner where only one of the four cells is wall). */
static void CornerTrims(Geo *g)
{
    const World *w = g->w;
    int x, z;
    for (z = 1; z < w->h; z++) {
        for (x = 1; x < w->w; x++) {
            int solid = 0, i, sx = 0, sz = 0;
            float top = 1e9f;
            static const int ox[4] = { -1, 0, -1, 0 }, oz[4] = { -1, -1, 0, 0 };
            for (i = 0; i < 4; i++) {
                int cx = x + ox[i], cz = z + oz[i];
                if (!IsOpen(w, cx, cz)) { solid++; sx = cx; sz = cz; }
                else top = fminf(top, CellCeiling(w, cx, cz));
            }
            if (solid != 1 || !IsPlainWall(w, sx, sz) || top > 100.0f) continue;
            BoxAt(g, MAT_TRIM, (float)x, (float)z, 0.09f, 0.09f, 0.0f, top);
        }
    }
}

/* Gold edge along a carpet runner where it meets other floor. */
static void CarpetEdges(Geo *g, int x, int z)
{
    const World *w = g->w;
    int d;
    for (d = 0; d < 4; d++) {
        int nx = x + DX[d], nz = z + DZ[d];
        Vector2 A, B, n;
        if (IsOpen(w, nx, nz) && w->floorMat[nz][nx] == MAT_CARPET) continue;
        EdgeOf(x, z, d, &A, &B, &n);
        {
            Vector2 a = Vector2Add(A, Vector2Scale(n, 0.0f)), b = Vector2Add(B, Vector2Scale(n, 0.0f));
            Vector2 a2 = Vector2Add(A, Vector2Scale(n, 0.07f)), b2 = Vector2Add(B, Vector2Scale(n, 0.07f));
            Quad(g, MAT_GOLD, (Vector3){ a.x, 0.016f, a.y }, (Vector3){ a2.x, 0.016f, a2.y },
                 (Vector3){ b2.x, 0.016f, b2.y }, (Vector3){ b.x, 0.016f, b.y });
        }
    }
}

static void Floor(Geo *g, int x, int z)
{
    const World *w = g->w;
    int mat = w->floorMat[z][x];
    float y = mat == MAT_CARPET ? 0.012f : 0.0f;
    Quad(g, mat, (Vector3){ (float)x, y, z + 1.0f }, (Vector3){ x + 1.0f, y, z + 1.0f },
         (Vector3){ x + 1.0f, y, (float)z }, (Vector3){ (float)x, y, (float)z });
    if (mat == MAT_CARPET) CarpetEdges(g, x, z);
}

/* ============================================================ sconces + bones */

static Vector3 TorchFlame(int x, int z, int d)
{
    return (Vector3){ x + 0.5f + DX[d] * 0.68f, TORCH_HEIGHT, z + 0.5f + DZ[d] * 0.68f };
}

/* Iron wall bracket holding a torch (the flame itself is drawn every frame, it flickers). */
static void Sconce(Geo *g, int x, int z, int d)
{
    Vector3 n = { (float)DX[d], 0.0f, (float)DZ[d] };
    Vector3 wall = { x + 0.5f + n.x * 0.5f, TORCH_HEIGHT, z + 0.5f + n.z * 0.5f };
    Vector3 cup = Vector3Add(wall, Vector3Scale(n, 0.18f));
    float px = fabsf(n.x) * 0.03f + fabsf(n.z) * 0.10f, pz = fabsf(n.z) * 0.03f + fabsf(n.x) * 0.10f;
    Vector3 plate = Vector3Add(wall, Vector3Scale(n, 0.03f)), arm = Vector3Add(wall, Vector3Scale(n, 0.10f));
    BoxAt(g, MAT_METAL, plate.x, plate.z, px, pz, TORCH_HEIGHT - 0.55f, TORCH_HEIGHT - 0.15f);             /* back plate */
    BoxAt(g, MAT_METAL, arm.x, arm.z, 0.03f + fabsf(n.x) * 0.07f, 0.03f + fabsf(n.z) * 0.07f,
          TORCH_HEIGHT - 0.40f, TORCH_HEIGHT - 0.34f);                                                     /* arm */
    BoxAt(g, MAT_METAL, cup.x, cup.z, 0.07f, 0.07f, TORCH_HEIGHT - 0.20f, TORCH_HEIGHT - 0.10f);           /* cup */
    BoxAt(g, MAT_WOOD, cup.x, cup.z, 0.04f, 0.04f, TORCH_HEIGHT - 0.45f, TORCH_HEIGHT - 0.06f);            /* torch */
}

static void CollectTorches(World *w)
{
    int x, z, d;
    w->torchCount = 0;
    w->flameCount = 0;
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            if (w->grid[z][x] != 'T') continue;
            for (d = 0; d < 4; d++) {
                int nx = x + DX[d], nz = z + DZ[d];
                if (!IsOpen(w, nx, nz) || w->grid[nz][nx] == 'E' || w->torchCount >= MAX_TORCHES) continue;
                w->torches[w->torchCount].pos = TorchFlame(x, z, d);
                w->torches[w->torchCount].normal = (Vector3){ (float)DX[d], 0.0f, (float)DZ[d] };
                w->torches[w->torchCount].color = (Vector3){ TORCH_COLOR_R, TORCH_COLOR_G, TORCH_COLOR_B };
                w->torches[w->torchCount].radius = TORCH_RADIUS;
                w->torches[w->torchCount].flicker = 1.0f;
                w->torchCount++;
                World_AddFlame(w, TorchFlame(x, z, d), 1.0f);
            }
        }
    }
}

/* 'x': a pile of bones with a skull on top. */
static void Bones(Geo *g, int x, int z)
{
    int i;
    for (i = 0; i < 9; i++) {
        float px = x + 0.2f + 0.6f * Hash01(x, z, i * 3 + 1), pz = z + 0.2f + 0.6f * Hash01(x, z, i * 3 + 2);
        float r = Hash01(x, z, i * 3 + 3), y = (i % 3) * 0.04f;
        if (r < 0.5f) BoxAt(g, MAT_BONE, px, pz, 0.2f, 0.035f, y, y + 0.06f);
        else          BoxAt(g, MAT_BONE, px, pz, 0.035f, 0.2f, y, y + 0.06f);
    }
    BoxAt(g, MAT_BONE, x + 0.5f, z + 0.5f, 0.12f, 0.11f, 0.10f, 0.32f);        /* skull */
    BoxAt(g, MAT_METAL, x + 0.45f, z + 0.61f, 0.035f, 0.005f, 0.20f, 0.25f);   /* eye sockets */
    BoxAt(g, MAT_METAL, x + 0.55f, z + 0.61f, 0.035f, 0.005f, 0.20f, 0.25f);
}

/* ============================================================ chunks */

static void BuildChunk(Geo *g, int cx, int cz)
{
    const World *w = g->w;
    int x, z, d;
    for (z = cz * CHUNK_SIZE; z < (cz + 1) * CHUNK_SIZE && z < w->h; z++) {
        for (x = cx * CHUNK_SIZE; x < (cx + 1) * CHUNK_SIZE && x < w->w; x++) {
            char c = w->grid[z][x];
            if (!IsOpen(w, x, z)) {
                if (c == 'B' && w->island[z][x])           /* top of a free-standing bookcase */
                    Quad(g, MAT_WOOD, (Vector3){ (float)x, BOOKSHELF_HEIGHT + 0.12f, z + 1.0f }, (Vector3){ x + 1.0f, BOOKSHELF_HEIGHT + 0.12f, z + 1.0f },
                         (Vector3){ x + 1.0f, BOOKSHELF_HEIGHT + 0.12f, (float)z }, (Vector3){ (float)x, BOOKSHELF_HEIGHT + 0.12f, (float)z });
                if (c == 'T')
                    for (d = 0; d < 4; d++)
                        if (IsOpen(w, x + DX[d], z + DZ[d]) && w->grid[z + DZ[d]][x + DX[d]] != 'E') Sconce(g, x, z, d);
                continue;
            }
            Floor(g, x, z);
            CeilingCell(g, x, z);
            Beams(g, x, z);
            if (c == 'P') Column(g, x, z);
            if (c == 'x') Bones(g, x, z);
            for (d = 0; d < 4; d++) {
                int nx = x + DX[d], nz = z + DZ[d];
                if (!IsOpen(w, nx, nz)) {
                    WallEdge(g, x, z, d);
                    Pilasters(g, x, z, d);
                } else if (w->area[z][x] == AREA_ROOM && w->area[nz][nx] == AREA_CORRIDOR) {
                    LintelEdge(g, x, z, d);
                }
            }
        }
    }
}

/* Things that are not tied to one chunk (arches, vault ribs, corner trims) go into chunk 0. */
static void BuildGlobal(Geo *g)
{
    int i;
    BuildArches(g);
    for (i = 0; i < g->w->roomCount; i++) {
        const Room *r = &g->w->rooms[i];
        if (r->x1 - r->x0 + 1 >= VAULT_MIN_ROOM && r->z1 - r->z0 + 1 >= VAULT_MIN_ROOM) VaultRibs(g, r, i + 1);
    }
    CornerTrims(g);
    Props_Build(g);
}

static void FlushParts(Geo *g, World *w)
{
    int m;
    bool any = false;
    for (m = 0; m < MAT_COUNT; m++) {
        Mesh mesh = MB_End(&g->mb[m]);
        if (mesh.vertexCount == 0) continue;
        if (w->partCount >= MAX_WORLD_PARTS) { UnloadMesh(mesh); continue; }
        w->parts[w->partCount].mesh = mesh;
        w->parts[w->partCount].mat = m;
        w->partCount++;
        w->vertexCount += mesh.vertexCount;
        any = true;
    }
    if (any) w->chunkCount++;
}

void World_BuildMeshes(World *w)
{
    static Geo g;
    int cx, cz, m, x, z;
    int ncx = (w->w + CHUNK_SIZE - 1) / CHUNK_SIZE, ncz = (w->h + CHUNK_SIZE - 1) / CHUNK_SIZE;

    w->partCount = 0;
    w->chunkCount = 0;
    w->vertexCount = 0;
    memset(lightCache, 0, sizeof(lightCache));
    CollectTorches(w);
    w->propCount = 0;
    w->colliderCount = 0;
    for (z = 0; z < w->h; z++)
        for (x = 0; x < w->w; x++) w->ceiling[z][x] = IsOpen(w, x, z) ? CellCeiling(w, x, z) : 0.0f;
    FindWindows(w);                 /* before props (they keep clear of windows) */
    Props_Place(w);                 /* before baking: candles and lanterns light the walls too */
    g.w = w;
    for (cz = 0; cz < ncz; cz++) {
        for (cx = 0; cx < ncx; cx++) {
            for (m = 0; m < MAT_COUNT; m++) MB_Begin(&g.mb[m]);
            BuildChunk(&g, cx, cz);
            FlushParts(&g, w);
        }
    }
    for (m = 0; m < MAT_COUNT; m++) MB_Begin(&g.mb[m]);
    BuildGlobal(&g);
    FlushParts(&g, w);
}

void World_Unload(World *w)
{
    int i;
    for (i = 0; i < w->partCount; i++) UnloadMesh(w->parts[i].mesh);
    w->partCount = 0;
}
