/* props.c - furniture and decoration (CLAUDE.md 7.4).
 * Props_Place decides, deterministically per wing, what stands where: props go against walls of
 * room cells (wall "slots") or in free floor areas, never on chests, spawns, the start, carpets
 * or the lanes in front of openings and doors. Every solid prop adds a collision box, and a prop
 * that would cut off any part of the wing (flood fill from the start) is thrown away again.
 * Candles, lanterns, chandeliers and fireplaces add flames and light sources.
 * Props_Build then turns the list into low-poly geometry (boxes, lathes, a few quads). */
#include <string.h>
#include <math.h>
#include "geo.h"
#include "config.h"
#include "raymath.h"

enum {
    P_CANDELABRA = 0, P_ARMOR, P_STONE_BENCH, P_BANNER, P_PAINTING, P_RUG, P_RUBBLE, P_COBWEB,
    P_LANTERN_HANG, P_LANTERN_WALL, P_BENCH, P_TABLE, P_CHANDELIER, P_BED, P_TRUNK, P_FIREPLACE,
    P_BOOKCASE, P_DESK, P_GLOBE, P_STAIR, P_GALLERY, P_JAR_SHELF, P_CAULDRON, P_WORKBENCH, P_STOOL,
    P_COFFIN, P_SKULL_NICHE, P_MIRROR, P_PEW, P_BELL, P_ROPE, P_PORTCULLIS, P_CANDLES
};

static const int DX[4] = { 1, -1, 0, 0 };
static const int DZ[4] = { 0, 0, 1, -1 };

static unsigned char lane[WORLD_MAX_H][WORLD_MAX_W];   /* keep free (paths, chests, spawns...) */
static unsigned char used[WORLD_MAX_H][WORLD_MAX_W];   /* a prop already stands here */
static unsigned char slotUsed[WORLD_MAX_H][WORLD_MAX_W][4];
static int reachCount;                                 /* reachable cells with the props so far */

/* ============================================================ helpers */

static float Hash01(int x, int y, int seed)
{
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ (unsigned int)seed * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
}

static bool InBounds(const World *w, int x, int z) { return x >= 0 && z >= 0 && x < w->w && z < w->h; }
static bool IsOpen(const World *w, int x, int z) { return InBounds(w, x, z) && w->area[z][x] != AREA_SOLID; }
static bool IsRoom(const World *w, int x, int z) { return IsOpen(w, x, z) && w->area[z][x] == AREA_ROOM; }

static bool IsPlainWall(const World *w, int x, int z)
{
    char c;
    if (!InBounds(w, x, z) || w->island[z][x] || w->window[z][x]) return false;
    c = w->grid[z][x];
    return c == '#' || c == 'W';
}

static bool IsVaultedRoom(const World *w, const Room *r)
{
    return r->x1 - r->x0 + 1 >= VAULT_MIN_ROOM && r->z1 - r->z0 + 1 >= VAULT_MIN_ROOM;
}

/* Wall slot: room cell (x,z) with a plain wall in direction d. Frame: origin on the wall line,
 * local +z points out of the wall into the room. */
static Frame SlotFrame(int x, int z, int d)
{
    Frame f;
    f.o = (Vector3){ x + 0.5f + DX[d] * 0.5f, 0.0f, z + 0.5f + DZ[d] * 0.5f };
    f.yaw = atan2f((float)-DX[d], (float)-DZ[d]);
    return f;
}

static bool FreeCell(const World *w, int x, int z)
{
    return IsOpen(w, x, z) && !lane[z][x] && !used[z][x] && w->grid[z][x] != 'P';
}

/* ---- reachability: flood fill from the start over cells not covered by a prop ---- */

static bool CellFree(const World *w, int x, int z)
{
    return !World_IsSolid(w, x, z) && !World_PropBlocked(w, x + 0.5f, z + 0.5f, 0.25f);
}

static int Reach(const World *w, bool *ok)
{
    static short qx[WORLD_MAX_W * WORLD_MAX_H], qz[WORLD_MAX_W * WORLD_MAX_H];
    static unsigned char seen[WORLD_MAX_H][WORLD_MAX_W];
    int head = 0, tail = 0, d, i, x = (int)w->start.x, z = (int)w->start.z;
    memset(seen, 0, sizeof(seen));
    *ok = true;
    if (!CellFree(w, x, z)) { *ok = false; return 0; }
    seen[z][x] = 1;
    qx[tail] = (short)x; qz[tail] = (short)z; tail++;
    while (head < tail) {
        int cx = qx[head], cz = qz[head];
        head++;
        for (d = 0; d < 4; d++) {
            int nx = cx + DX[d], nz = cz + DZ[d];
            if (!InBounds(w, nx, nz) || seen[nz][nx] || !CellFree(w, nx, nz)) continue;
            seen[nz][nx] = 1;
            qx[tail] = (short)nx; qz[tail] = (short)nz; tail++;
        }
    }
    /* every chest and exit still needs a reachable neighbour */
    for (i = 0; i < w->chestCount + w->exitCount; i++) {
        Cell c = i < w->chestCount ? w->chests[i] : w->exits[i - w->chestCount];
        bool any = false;
        for (d = 0; d < 4; d++) if (InBounds(w, c.x + DX[d], c.z + DZ[d]) && seen[c.z + DZ[d]][c.x + DX[d]]) any = true;
        if (!any) *ok = false;
    }
    return tail;
}

/* Cells whose centre lies inside a box (they become blocked for the flood fill). */
static int CoveredCells(const World *w, Collider c)
{
    int n = 0, x, z;
    for (z = (int)floorf(c.z0); z <= (int)floorf(c.z1); z++)
        for (x = (int)floorf(c.x0); x <= (int)floorf(c.x1); x++)
            if (InBounds(w, x, z) && !World_IsSolid(w, x, z) &&
                x + 0.5f > c.x0 - 0.25f && x + 0.5f < c.x1 + 0.25f && z + 0.5f > c.z0 - 0.25f && z + 0.5f < c.z1 + 0.25f) {
                bool before = World_PropBlocked(w, x + 0.5f, z + 0.5f, 0.25f);
                if (!before) n++;
            }
    return n;
}

/* Collision box of a prop from its frame and local half sizes (x across, z out of the wall). */
static Collider BoxOf(Frame f, float cx, float cz, float hx, float hz)
{
    Vector3 a = Frame_Point(f, cx - hx, 0, cz - hz), b = Frame_Point(f, cx + hx, 0, cz + hz);
    Collider c = { fminf(a.x, b.x), fminf(a.z, b.z), fmaxf(a.x, b.x), fmaxf(a.z, b.z) };
    return c;
}

/* Add a prop. Solid props (hx > 0) get a collision box and are rejected if they would make any
 * part of the wing unreachable. Returns the prop or NULL. */
static Prop *Add(World *w, int type, Frame f, int cx, int cz, float param, float ox, float oz, float hx, float hz)
{
    Prop *p;
    if (w->propCount >= MAX_PROPS) return NULL;
    if (hx > 0.0f) {
        Collider c = BoxOf(f, ox, oz, hx, hz);
        int lost;
        bool ok;
        if (w->colliderCount >= MAX_COLLIDERS) return NULL;
        lost = CoveredCells(w, c);
        w->colliders[w->colliderCount++] = c;
        if (Reach(w, &ok) != reachCount - lost || !ok) { w->colliderCount--; return NULL; }
        reachCount -= lost;
    }
    p = &w->props[w->propCount++];
    p->type = type;
    p->x = f.o.x;
    p->z = f.o.z;
    p->yaw = f.yaw;
    p->cx = cx;
    p->cz = cz;
    p->param = param;
    used[cz][cx] = 1;
    return p;
}

static Vector3 Amber(float k) { return (Vector3){ TORCH_COLOR_R * k, TORCH_COLOR_G * k, TORCH_COLOR_B * k }; }

/* ============================================================ lanes */

static void Mark(int x, int z, int r, const World *w)
{
    int i, j;
    for (j = -r; j <= r; j++)
        for (i = -r; i <= r; i++)
            if (InBounds(w, x + i, z + j)) lane[z + j][x + i] = 1;
}

static void MarkLanes(const World *w)
{
    int x, z, d, k, i;
    memset(lane, 0, sizeof(lane));
    Mark((int)w->start.x, (int)w->start.z, 1, w);
    for (i = 0; i < w->chestCount; i++) Mark(w->chests[i].x, w->chests[i].z, 1, w);
    for (i = 0; i < w->spawnCount; i++) Mark((int)w->spawns[i].pos.x, (int)w->spawns[i].pos.z, 0, w);
    for (i = 0; i < w->npcCount; i++) Mark((int)w->npcs[i].pos.x, (int)w->npcs[i].pos.z, 1, w);
    for (i = 0; i < w->exitCount; i++) Mark(w->exits[i].x, w->exits[i].z, 2, w);
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            char c = w->grid[z][x];
            if (c == '=' || c == 'x') lane[z][x] = 1;
            if (!IsRoom(w, x, z)) continue;
            /* openings into corridors: keep a 2-cell path in front of them clear */
            for (d = 0; d < 4; d++) {
                int nx = x + DX[d], nz = z + DZ[d];
                if (!IsOpen(w, nx, nz) || w->area[nz][nx] != AREA_CORRIDOR) continue;
                for (k = 0; k <= 2; k++) Mark(x - DX[d] * k, z - DZ[d] * k, 1, w);
            }
        }
    }
}

/* ============================================================ placement helpers */

/* Every free wall slot of a room, visited in a stable pseudo-random order: calls fn(x, z, d). */
typedef bool (*SlotFn)(World *w, int x, int z, int d, void *ctx);

static void ForSlots(World *w, int roomId, int seed, SlotFn fn, void *ctx)
{
    int x, z, d, k;
    /* a fixed pseudo-random start keeps placements spread out but deterministic */
    int total = w->w * w->h * 4, start = (int)(Hash01(roomId, seed, 7) * total);
    for (k = 0; k < total; k++) {
        int idx = (start + k * 7919) % total;
        x = (idx / 4) % w->w;
        z = (idx / 4) / w->w;
        d = idx % 4;
        if (!IsRoom(w, x, z) || (roomId > 0 && w->roomId[z][x] != roomId)) continue;
        if (lane[z][x] || used[z][x] || slotUsed[z][x][d] || !IsPlainWall(w, x + DX[d], z + DZ[d])) continue;
        if (fn(w, x, z, d, ctx)) slotUsed[z][x][d] = 1;
    }
}

typedef struct { int type, left, every, seed; } SlotJob;

/* Simple wall props: candelabra, armor, benches, trunks, bookcases, jar shelves, ... */
static bool PlaceSimple(World *w, int x, int z, int d, void *ctx)
{
    SlotJob *j = (SlotJob *)ctx;
    Frame f = SlotFrame(x, z, d);
    Prop *p = NULL;
    if (j->left <= 0 || Hash01(x * 4 + d, z, j->seed) * j->every > 1.0f) return false;
    switch (j->type) {
    case P_CANDELABRA:  p = Add(w, j->type, f, x, z, 0, 0, 0.45f, 0.22f, 0.22f); break;
    case P_ARMOR:       p = Add(w, j->type, f, x, z, 0, 0, 0.35f, 0.36f, 0.32f); break;
    case P_STONE_BENCH: p = Add(w, j->type, f, x, z, 0, 0, 0.30f, 0.45f, 0.25f); break;
    case P_BENCH:       p = Add(w, j->type, f, x, z, 0, 0, 0.30f, 0.45f, 0.22f); break;
    case P_TRUNK:       p = Add(w, j->type, f, x, z, 0, 0, 0.35f, 0.42f, 0.28f); break;
    case P_BOOKCASE:    p = Add(w, j->type, f, x, z, 0, 0, 0.24f, 0.5f, 0.24f); break;
    case P_JAR_SHELF:   p = Add(w, j->type, f, x, z, 0, 0, 0.24f, 0.5f, 0.24f); break;
    case P_WORKBENCH:   p = Add(w, j->type, f, x, z, 0, 0, 0.40f, 0.48f, 0.38f); break;
    case P_MIRROR:      p = Add(w, j->type, f, x, z, 0, 0, 0.50f, 0.50f, 0.28f); break;
    default:            p = Add(w, j->type, f, x, z, 0, 0, 0, 0, 0); break;   /* flat on the wall */
    }
    if (!p) return false;
    j->left--;
    if (j->type == P_CANDELABRA) {
        int k;
        for (k = -1; k <= 1; k++) World_AddFlame(w, Frame_Point(f, k * 0.25f, 1.62f, 0.45f), 0.35f);
        World_AddLight(w, Frame_Point(f, 0, 1.7f, 0.6f), Amber(0.9f), 5.5f);
    }
    return true;
}

static void SlotProps(World *w, int roomId, int type, int count, float every, int seed)
{
    SlotJob j = { type, count, (int)every, seed };
    ForSlots(w, roomId, seed, PlaceSimple, &j);
}

/* Is a free floor rectangle (cells x0..x1, z0..x1) available for a centre prop? */
static bool FreeRect(const World *w, int x0, int z0, int x1, int z1)
{
    int x, z;
    for (z = z0; z <= z1; z++)
        for (x = x0; x <= x1; x++)
            if (!FreeCell(w, x, z)) return false;
    return true;
}

static void UseRect(int x0, int z0, int x1, int z1, int margin)
{
    int x, z;
    for (z = z0 - margin; z <= z1 + margin; z++)
        for (x = x0 - margin; x <= x1 + margin; x++)
            if (x >= 0 && z >= 0 && x < WORLD_MAX_W && z < WORLD_MAX_H) used[z][x] = 1;
}

/* ============================================================ everywhere */

/* Cobwebs in the upper inner corners of rooms. */
static void Cobwebs(World *w)
{
    int x, z;
    for (z = 1; z < w->h - 1; z++) {
        for (x = 1; x < w->w - 1; x++) {
            int a, b;
            if (!IsRoom(w, x, z)) continue;
            for (a = 0; a < 2; a++) for (b = 2; b < 4; b++) {
                Frame f;
                Prop *p;
                if (IsOpen(w, x + DX[a], z) || IsOpen(w, x, z + DZ[b])) continue;     /* need two walls */
                if (Hash01(x * 3 + a, z * 5 + b, 11) > 0.6f) continue;
                f.o = (Vector3){ x + 0.5f + DX[a] * 0.5f, 0.0f, z + 0.5f + DZ[b] * 0.5f };
                f.yaw = 0.0f;
                p = Add(w, P_COBWEB, f, x, z, (float)(a * 4 + b), 0, 0, 0, 0);
                if (p) used[z][x] = 0;                 /* cobwebs hang high: the floor stays free */
            }
        }
    }
}

/* Banners, paintings and a little rubble on room walls. */
static void WallDecor(World *w, int roomId, int seed)
{
    int x, z, d;
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            if (!IsRoom(w, x, z) || w->roomId[z][x] != roomId) continue;
            for (d = 0; d < 4; d++) {
                bool alongX = DZ[d] != 0;
                int along = alongX ? x : z;
                float r = Hash01(x * 4 + d, z, seed + 3);
                Frame f;
                Prop *p = NULL;
                if (slotUsed[z][x][d] || !IsPlainWall(w, x + DX[d], z + DZ[d])) continue;
                f = SlotFrame(x, z, d);
                if (along % 4 == 0 && r < 0.75f) p = Add(w, P_BANNER, f, x, z, 0, 0, 0, 0, 0);
                else if (along % 4 == 1 && r < 0.35f) p = Add(w, P_PAINTING, f, x, z, 0, 0, 0, 0, 0);
                else if (r > 0.9f && !lane[z][x]) p = Add(w, P_RUBBLE, f, x, z, 0, 0, 0, 0, 0);
                if (p) { slotUsed[z][x][d] = 1; used[z][x] = 0; }
            }
        }
    }
}

/* Corridors: hanging lanterns about every 4 cells, wall lanterns and benches along the walls. */
static void CorridorProps(World *w)
{
    int x, z, d, i;
    for (z = 1; z < w->h - 1; z++) {
        for (x = 1; x < w->w - 1; x++) {
            bool near = false;
            Frame f;
            if (!IsOpen(w, x, z) || w->area[z][x] != AREA_CORRIDOR || w->grid[z][x] == 'E') continue;
            for (i = 0; i < w->propCount && !near; i++) {
                const Prop *p = &w->props[i];
                if (p->type == P_LANTERN_HANG && fabsf(p->x - (x + 0.5f)) + fabsf(p->z - (z + 0.5f)) < 4.0f) near = true;
            }
            if (near) continue;
            f.o = (Vector3){ x + 0.5f, 0.0f, z + 0.5f };
            f.yaw = 0.0f;
            if (Add(w, P_LANTERN_HANG, f, x, z, w->ceiling[z][x], 0, 0, 0, 0)) {
                World_AddFlame(w, (Vector3){ x + 0.5f, 2.42f, z + 0.5f }, 0.35f);
                World_AddLight(w, (Vector3){ x + 0.5f, 2.3f, z + 0.5f }, Amber(1.0f), 5.5f);
                used[z][x] = 0;
            }
        }
    }
    for (z = 1; z < w->h - 1; z++) {
        for (x = 1; x < w->w - 1; x++) {
            if (!IsOpen(w, x, z) || w->area[z][x] != AREA_CORRIDOR || w->grid[z][x] == 'E' || lane[z][x]) continue;
            for (d = 0; d < 4; d++) {
                int ox = x - DX[d], oz = z - DZ[d];                 /* the corridor continues opposite */
                float r = Hash01(x * 4 + d, z, 21);
                Frame f;
                if (!IsPlainWall(w, x + DX[d], z + DZ[d]) || !IsOpen(w, ox, oz) || used[z][x]) continue;
                f = SlotFrame(x, z, d);
                if (r < 0.12f) {
                    if (Add(w, P_LANTERN_WALL, f, x, z, 0, 0, 0, 0, 0)) {
                        World_AddFlame(w, Frame_Point(f, 0, 2.18f, 0.28f), 0.3f);
                        World_AddLight(w, Frame_Point(f, 0, 2.1f, 0.5f), Amber(0.8f), 4.5f);
                        used[z][x] = 0;
                    }
                } else if (r > 0.9f) {
                    Add(w, P_BENCH, f, x, z, 0, 0, 0.25f, 0.45f, 0.2f);
                }
            }
        }
    }
}

/* ============================================================ centre props */

/* Long tables with benches and candles in a room, aisles kept free. */
static void Tables(World *w, const Room *r, int roomId, int maxTables, bool reading)
{
    bool alongX = (r->x1 - r->x0) >= (r->z1 - r->z0);
    int placed = 0, a, b;
    int len = reading ? 3 : 4;
    for (b = (alongX ? r->z0 : r->x0) + 1; b <= (alongX ? r->z1 : r->x1) - 1 && placed < maxTables; b++) {
        for (a = (alongX ? r->x0 : r->z0) + 1; a + len - 1 <= (alongX ? r->x1 : r->z1) - 1 && placed < maxTables; a++) {
            int x0 = alongX ? a : b - 1, z0 = alongX ? b - 1 : a;
            int x1 = alongX ? a + len - 1 : b + 1, z1 = alongX ? b + 1 : a + len - 1;
            Frame f;
            int k;
            if (w->roomId[(z0 + z1) / 2][(x0 + x1) / 2] != roomId || !FreeRect(w, x0, z0, x1, z1)) continue;
            f.o = (Vector3){ (x0 + x1 + 1) * 0.5f, 0.0f, (z0 + z1 + 1) * 0.5f };
            f.yaw = alongX ? 0.0f : PI * 0.5f;                    /* local x = along the table */
            if (!Add(w, reading ? P_DESK : P_TABLE, f, x0, z0, (float)len - 0.4f, 0, 0, len * 0.5f - 0.2f, reading ? 0.6f : 1.05f)) continue;
            UseRect(x0, z0, x1, z1, 1);
            for (k = 0; k < (reading ? 2 : 3); k++) {
                float px = (k - (reading ? 0.5f : 1.0f)) * (reading ? 1.2f : 1.3f);
                World_AddFlame(w, Frame_Point(f, px, 1.02f, 0.0f), 0.3f);
            }
            World_AddLight(w, Frame_Point(f, 0, 1.4f, 0), Amber(0.9f), 5.0f);
            placed++;
        }
    }
}

/* Iron ring chandeliers hanging over the middle of a room (one per ~70 cells, at most 3). */
static void Chandeliers(World *w, const Room *r, int count)
{
    bool alongX = (r->x1 - r->x0) >= (r->z1 - r->z0);
    int i;
    for (i = 0; i < count; i++) {
        float t = (i + 0.5f) / count;
        Frame f;
        float top, y;
        f.o = alongX ? (Vector3){ r->x0 + (r->x1 - r->x0 + 1) * t, 0, (r->z0 + r->z1 + 1) * 0.5f }
                     : (Vector3){ (r->x0 + r->x1 + 1) * 0.5f, 0, r->z0 + (r->z1 - r->z0 + 1) * t };
        f.yaw = 0.0f;
        top = World_CeilingAt(w, f.o.x, f.o.z);
        y = top - 1.9f;
        if (Add(w, P_CHANDELIER, f, (int)f.o.x, (int)f.o.z, top, 0, 0, 0, 0)) {
            int k;
            used[(int)f.o.z][(int)f.o.x] = 0;
            for (k = 0; k < 8; k++)
                World_AddFlame(w, (Vector3){ f.o.x + cosf(k * PI / 4) * 0.95f, y + 0.26f, f.o.z + sinf(k * PI / 4) * 0.95f }, 0.3f);
            World_AddLight(w, (Vector3){ f.o.x, y, f.o.z }, Amber(1.25f), 9.0f);
        }
    }
}

/* Four-poster beds against walls (head at the wall), with a trunk at the foot. */
static bool PlaceBed(World *w, int x, int z, int d, void *ctx)
{
    int *left = (int *)ctx, fx = x - DX[d], fz = z - DZ[d];
    Frame f = SlotFrame(x, z, d);
    if (*left <= 0 || !FreeCell(w, fx, fz) || !FreeCell(w, fx - DX[d], fz - DZ[d])) return false;
    if (!Add(w, P_BED, f, x, z, 0, 0, 1.1f, 0.76f, 1.1f)) return false;
    used[fz][fx] = 1;
    used[fz - DZ[d]][fx - DX[d]] = 1;
    (*left)--;
    return true;
}

/* A stone fireplace needs three wall slots in a row; a rug lies in front of it. */
static bool PlaceFireplace(World *w, int x, int z, int d, void *ctx)
{
    int *left = (int *)ctx, sx = DZ[d] != 0 ? 1 : 0, sz = DX[d] != 0 ? 1 : 0;
    Frame f = SlotFrame(x, z, d);
    Prop *p;
    if (*left <= 0) return false;
    if (!IsRoom(w, x - sx, z - sz) || !IsRoom(w, x + sx, z + sz) || lane[z - sz][x - sx] || lane[z + sz][x + sx]) return false;
    if (!IsPlainWall(w, x - sx + DX[d], z - sz + DZ[d]) || !IsPlainWall(w, x + sx + DX[d], z + sz + DZ[d])) return false;
    p = Add(w, P_FIREPLACE, f, x, z, 0, 0, 0.38f, 1.05f, 0.38f);
    if (!p) return false;
    used[z - sz][x - sx] = used[z + sz][x + sx] = 1;
    slotUsed[z - sz][x - sx][d] = slotUsed[z + sz][x + sx][d] = 1;
    World_AddFlame(w, Frame_Point(f, -0.22f, 0.32f, 0.32f), 1.1f);
    World_AddFlame(w, Frame_Point(f, 0.0f, 0.38f, 0.30f), 1.3f);
    World_AddFlame(w, Frame_Point(f, 0.22f, 0.32f, 0.34f), 1.0f);
    World_AddLight(w, Frame_Point(f, 0, 0.9f, 0.9f), (Vector3){ 0.9f, 0.5f, 0.22f }, 7.0f);
    Add(w, P_RUG, f, x, z, 0, 0, 0, 0, 0);
    (*left)--;
    return true;
}

/* A few candles standing on the floor against a wall (crypts, the Sanctum). */
static bool PlaceCandles(World *w, int x, int z, int d, void *ctx)
{
    int *left = (int *)ctx, k;
    Frame f = SlotFrame(x, z, d);
    if (*left <= 0 || Hash01(x, z * 4 + d, 31) > 0.3f) return false;
    if (!Add(w, P_CANDLES, f, x, z, 0, 0, 0, 0, 0)) return false;
    used[z][x] = 0;
    for (k = 0; k < 3; k++) World_AddFlame(w, Frame_Point(f, -0.2f + k * 0.2f, 0.32f + k * 0.12f, 0.25f + (k & 1) * 0.1f), 0.28f);
    World_AddLight(w, Frame_Point(f, 0, 0.6f, 0.5f), Amber(0.7f), 4.0f);
    (*left)--;
    return true;
}

static void Run(World *w, int roomId, SlotFn fn, int count, int seed)
{
    ForSlots(w, roomId, seed, fn, &count);
}

/* Centre props on free 1-cell spots of a room (coffins, cauldrons, pews, stools). */
static void CentreProps(World *w, const Room *r, int roomId, int type, int count, int seed)
{
    int x, z, k = 0;
    for (z = r->z0 + 1; z <= r->z1 - 1 && k < count; z++) {
        for (x = r->x0 + 1; x <= r->x1 - 1 && k < count; x++) {
            Frame f;
            Prop *p = NULL;
            if (w->roomId[z][x] != roomId || Hash01(x, z, seed) > 0.35f) continue;
            if (type == P_COFFIN || type == P_PEW) {
                bool alongX = (r->x1 - r->x0) >= (r->z1 - r->z0);
                int x1 = x + (alongX && type == P_COFFIN ? 1 : 0), z1 = z + (!alongX && type == P_COFFIN ? 1 : 0);
                if (type == P_PEW) { x1 = x + (alongX ? 0 : 1); z1 = z + (alongX ? 1 : 0); }
                if (!FreeRect(w, x, z, x1, z1)) continue;
                f.o = (Vector3){ (x + x1 + 1) * 0.5f, 0, (z + z1 + 1) * 0.5f };
                if (type == P_COFFIN) {
                    f.yaw = alongX ? PI * 0.5f : 0.0f;
                    p = Add(w, type, f, x, z, 0, 0, 0, 0.48f, 1.02f);
                } else {
                    f.yaw = (alongX ? PI * 0.5f : 0.0f) + (Hash01(x, z, seed + 1) - 0.5f) * 0.5f;    /* knocked askew */
                    p = Add(w, type, f, x, z, Hash01(x, z, seed + 2), 0, 0, 0.75f, 0.75f);
                }
                if (p) UseRect(x, z, x1, z1, 1);
            } else {
                if (!FreeRect(w, x, z, x, z)) continue;
                f.o = (Vector3){ x + 0.5f, 0, z + 0.5f };
                f.yaw = Hash01(x, z, seed + 3) * 2.0f * PI;
                p = Add(w, type, f, x, z, 0, 0, 0, type == P_STOOL ? 0.22f : 0.48f, type == P_STOOL ? 0.22f : 0.48f);
                if (p) {
                    UseRect(x, z, x, z, 1);
                    if (type == P_CAULDRON) World_AddLight(w, (Vector3){ x + 0.5f, 1.0f, z + 0.5f }, (Vector3){ 0.25f, 0.85f, 0.35f }, 4.0f);
                }
            }
            if (p) k++;
        }
    }
}

/* ============================================================ wing themes */

static int BiggestRoom(const World *w)
{
    int i, best = 0;
    for (i = 1; i < w->roomCount; i++) if (w->rooms[i].cells > w->rooms[best].cells) best = i;
    return best;
}

/* The start room's back wall gets an iron portcullis (the way the apprentice came in). */
static void Portcullis(World *w)
{
    int x = (int)w->start.x, z = (int)w->start.z, k, d;
    float yaw = w->startYaw;
    int bx = -(int)roundf(sinf(yaw)), bz = -(int)roundf(cosf(yaw));      /* behind the start */
    for (k = 1; k < 12; k++) {
        int cx = x + bx * k, cz = z + bz * k;
        if (!IsOpen(w, cx, cz)) {
            int px = cx - bx, pz = cz - bz;
            for (d = 0; d < 4; d++)
                if (DX[d] == bx && DZ[d] == bz && IsPlainWall(w, cx, cz)) {
                    Add(w, P_PORTCULLIS, SlotFrame(px, pz, d), px, pz, 0, 0, 0, 0, 0);
                    slotUsed[pz][px][d] = 1;
                    used[pz][px] = 0;
                }
            return;
        }
    }
}

static void ThemeAshGate(World *w)
{
    int i;
    Portcullis(w);
    for (i = 0; i < w->roomCount; i++) {
        SlotProps(w, i + 1, P_ARMOR, 4, 2.5f, 40 + i);
        SlotProps(w, i + 1, P_CANDELABRA, 4, 2.0f, 50 + i);
        SlotProps(w, i + 1, P_STONE_BENCH, 2, 3.0f, 60 + i);
    }
}

static void ThemeHallOfPrayer(World *w)
{
    int big = BiggestRoom(w), i, n;
    const Room *r = &w->rooms[big];
    Tables(w, r, big + 1, 6, false);
    Chandeliers(w, r, r->cells > 140 ? 3 : 2);
    for (i = 0; i < w->roomCount; i++) {
        if (i == big) { SlotProps(w, i + 1, P_CANDELABRA, 4, 2.0f, 70); continue; }
        n = 1;
        Run(w, i + 1, PlaceFireplace, 1, 80 + i);
        Run(w, i + 1, PlaceBed, 3, 90 + i);
        SlotProps(w, i + 1, P_TRUNK, 2, 2.0f, 100 + i);
        SlotProps(w, i + 1, P_CANDELABRA, 2, 2.0f, 110 + i);
        (void)n;
    }
}

/* Tower study: desk + globe in the middle, a spiral stair in a corner, a gallery all around. */
static void TowerStudy(World *w, const Room *r, int roomId)
{
    int cx = (r->x0 + r->x1) / 2, cz = (r->z0 + r->z1) / 2, x, z, d;
    Frame f;
    for (z = cz - 1; z <= cz + 1; z++)
        for (x = cx - 1; x <= cx + 1; x++) lane[z][x] = 0;
    if (FreeRect(w, cx - 1, cz, cx + 1, cz)) {
        f.o = (Vector3){ cx + 0.5f, 0, cz + 0.5f };
        f.yaw = 0.0f;
        if (Add(w, P_DESK, f, cx, cz, 2.0f, 0, 0, 1.0f, 0.6f)) {
            World_AddFlame(w, Frame_Point(f, 0.7f, 1.02f, 0.2f), 0.3f);
            World_AddLight(w, Frame_Point(f, 0, 1.5f, 0), Amber(1.0f), 5.5f);
            UseRect(cx - 1, cz, cx + 1, cz, 0);
        }
        f.o = (Vector3){ cx + 0.5f, 0, cz + 2.0f };
        if (FreeRect(w, cx, cz + 1, cx, cz + 1) && Add(w, P_GLOBE, f, cx, cz + 1, 0, 0, 0, 0.3f, 0.3f)) UseRect(cx, cz + 1, cx, cz + 1, 0);
    }
    /* spiral stair in the first free corner */
    for (d = 0; d < 4; d++) {
        int sx = d & 1 ? r->x1 - 1 : r->x0, sz = d & 2 ? r->z1 - 1 : r->z0;
        if (!FreeRect(w, sx, sz, sx + 1, sz + 1)) continue;
        f.o = (Vector3){ sx + 1.0f, 0, sz + 1.0f };
        f.yaw = 0.0f;
        if (Add(w, P_STAIR, f, sx, sz, World_CeilingAt(w, f.o.x, f.o.z), 0, 0, 0.75f, 0.75f)) { UseRect(sx, sz, sx + 1, sz + 1, 0); break; }
    }
    /* gallery ledge with a railing along every wall of the room, high up */
    for (z = r->z0; z <= r->z1; z++)
        for (x = r->x0; x <= r->x1; x++)
            for (d = 0; d < 4; d++)
                if (w->roomId[z][x] == roomId && !IsOpen(w, x + DX[d], z + DZ[d]) && !w->island[z + DZ[d]][x + DX[d]])
                    Add(w, P_GALLERY, SlotFrame(x, z, d), x, z, 0, 0, 0, 0, 0), used[z][x] = 0;
}

static void ThemeScriptorium(World *w)
{
    int big = BiggestRoom(w), i;
    for (i = 0; i < w->roomCount; i++) {
        if (i == big) { TowerStudy(w, &w->rooms[i], i + 1); continue; }
        Tables(w, &w->rooms[i], i + 1, 2, true);
        SlotProps(w, i + 1, P_BOOKCASE, 12, 1.2f, 120 + i);
        SlotProps(w, i + 1, P_CANDELABRA, 2, 2.0f, 130 + i);
    }
}

static void ThemeOssuary(World *w)
{
    int i, mirror = 1;
    for (i = 0; i < w->roomCount; i++) {
        const Room *r = &w->rooms[i];
        if (i % 2 == 0) {                                    /* alchemy */
            SlotProps(w, i + 1, P_JAR_SHELF, 6, 1.5f, 140 + i);
            SlotProps(w, i + 1, P_WORKBENCH, 2, 2.0f, 150 + i);
            CentreProps(w, r, i + 1, P_CAULDRON, 2, 160 + i);
            CentreProps(w, r, i + 1, P_STOOL, 3, 170 + i);
        } else {                                             /* crypt */
            CentreProps(w, r, i + 1, P_COFFIN, 4, 180 + i);
            SlotProps(w, i + 1, P_SKULL_NICHE, 6, 1.5f, 190 + i);
            Run(w, i + 1, PlaceCandles, 4, 200 + i);
            if (mirror > 0) { SlotProps(w, i + 1, P_MIRROR, 1, 1.0f, 210 + i); mirror = 0; }
        }
        SlotProps(w, i + 1, P_CANDELABRA, 1, 3.0f, 220 + i);
    }
}

/* The serpent's cage: a free 3x3 spot in the bell tower room, as far as possible from the Abbot. */
static void PlaceCage(World *w, const Room *r, int roomId)
{
    Vector3 boss = { (r->x0 + r->x1 + 1) * 0.5f, 0, (r->z0 + r->z1 + 1) * 0.5f };
    float best = -1.0f;
    int i, x, z, bx = -1, bz = -1;
    for (i = 0; i < w->spawnCount; i++) if (w->spawns[i].type == 'Q') boss = w->spawns[i].pos;
    for (z = r->z0 + 2; z <= r->z1 - 2; z++)
        for (x = r->x0 + 2; x <= r->x1 - 2; x++) {
            float d = Vector3Distance((Vector3){ x + 0.5f, 0, z + 0.5f }, boss);
            int i2, j2;
            bool ok = w->roomId[z][x] == roomId;
            for (j2 = -1; j2 <= 1 && ok; j2++)
                for (i2 = -1; i2 <= 1 && ok; i2++)
                    if (!IsOpen(w, x + i2, z + j2) || w->grid[z + j2][x + i2] == 'P' || World_IsSolid(w, x + i2, z + j2)) ok = false;
            if (ok && d > best) { best = d; bx = x; bz = z; }
        }
    if (bx < 0 || w->colliderCount >= MAX_COLLIDERS) return;
    w->hasCage = true;
    w->cagePos = (Vector3){ bx + 0.5f, 0.0f, bz + 0.5f };
    w->cageCollider = w->colliderCount;
    w->colliders[w->colliderCount++] = (Collider){ bx - 0.9f, bz - 0.9f, bx + 1.9f, bz + 1.9f };   /* the game resets it */
    for (z = bz - 2; z <= bz + 2; z++)
        for (x = bx - 2; x <= bx + 2; x++)
            if (InBounds(w, x, z)) lane[z][x] = 1;                   /* nothing else stands around it */
}

static void ThemeBellTower(World *w)
{
    int big = BiggestRoom(w), i;
    const Room *r = &w->rooms[big];
    Frame f;
    PlaceCage(w, r, big + 1);
    reachCount = Reach(w, (bool[]){ true });
    f.o = (Vector3){ (r->x0 + r->x1 + 1) * 0.5f, 0, (r->z0 + r->z1 + 1) * 0.5f };
    if (w->hasCage && Vector3Distance(f.o, w->cagePos) < 4.5f)       /* the bell never hangs over the cage */
        f.o = Vector3Add(f.o, Vector3Scale(Vector3Normalize(Vector3Subtract(f.o, w->cagePos)), 4.5f - Vector3Distance(f.o, w->cagePos)));
    f.yaw = 0.0f;
    if (Add(w, P_BELL, f, (int)f.o.x, (int)f.o.z, World_CeilingAt(w, f.o.x, f.o.z), 0, 0, 0, 0)) used[(int)f.o.z][(int)f.o.x] = 0;
    for (i = 0; i < 3; i++) {
        f.o = (Vector3){ r->x0 + 2.5f + i * 3.1f, 0, r->z0 + 2.5f + (i % 2) * 4.0f };
        if (IsRoom(w, (int)f.o.x, (int)f.o.z) && Add(w, P_ROPE, f, (int)f.o.x, (int)f.o.z, World_CeilingAt(w, f.o.x, f.o.z), 0, 0, 0, 0))
            used[(int)f.o.z][(int)f.o.x] = 0;
    }
    for (i = 0; i < w->roomCount; i++) {
        CentreProps(w, &w->rooms[i], i + 1, P_PEW, i == big ? 8 : 3, 230 + i);
        SlotProps(w, i + 1, P_CANDELABRA, 2, 2.5f, 240 + i);
        SlotProps(w, i + 1, P_RUBBLE, 6, 1.5f, 250 + i);
    }
}

static void ThemeSanctum(World *w)
{
    const Room *r = &w->rooms[BiggestRoom(w)];
    Chandeliers(w, r, 3);
    SlotProps(w, 0, P_CANDELABRA, 8, 1.5f, 260);
    Run(w, 0, PlaceCandles, 8, 270);
}

/* ============================================================ API: placement */

void Props_Place(World *w)
{
    bool ok;
    int i;
    memset(used, 0, sizeof(used));
    memset(slotUsed, 0, sizeof(slotUsed));
    w->hasCage = false;
    w->cageCollider = -1;
    MarkLanes(w);
    reachCount = Reach(w, &ok);
    switch (w->theme) {
    case 0: ThemeAshGate(w); break;
    case 1: ThemeHallOfPrayer(w); break;
    case 2: ThemeScriptorium(w); break;
    case 3: ThemeOssuary(w); break;
    case 4: ThemeBellTower(w); break;
    default: ThemeSanctum(w); break;
    }
    if (w->theme < WING_COUNT) {
        CorridorProps(w);
        for (i = 0; i < w->roomCount; i++) {
            SlotProps(w, i + 1, P_CANDELABRA, 1, 1.0f, 300 + i);   /* every room gets at least one light */
            WallDecor(w, i + 1, 310 + i);
        }
    } else {
        for (i = 0; i < w->roomCount; i++) WallDecor(w, i + 1, 320 + i);
    }
    Cobwebs(w);
}

/* ============================================================ geometry */

static void Candle(Geo *g, Vector3 base, float h)
{
    const float r[3] = { 0.035f, 0.035f, 0.03f }, y[3] = { 0.0f, h, h + 0.02f };
    Geo_Lathe(g, MAT_BONE, base, r, y, 3, 6, true);
}

static void BuildCandelabra(Geo *g, Frame f)
{
    const float r[6] = { 0.20f, 0.20f, 0.06f, 0.035f, 0.035f, 0.06f }, y[6] = { 0.0f, 0.05f, 0.12f, 0.2f, 1.36f, 1.42f };
    int k;
    Geo_Lathe(g, MAT_METAL, Frame_Point(f, 0, 0, 0.45f), r, y, 6, 6, true);
    Geo_FBox(g, MAT_METAL, f, 0, 1.40f, 0.45f, 0.27f, 0.02f, 0.02f);
    for (k = -1; k <= 1; k++) {
        Geo_FBox(g, MAT_METAL, f, k * 0.25f, 1.44f, 0.45f, 0.045f, 0.015f, 0.045f);
        Candle(g, Frame_Point(f, k * 0.25f, 1.45f, 0.45f), 0.12f);
    }
}

static void BuildArmor(Geo *g, Frame f)
{
    int s;
    Geo_FBox(g, MAT_TRIM, f, 0, 0.15f, 0.35f, 0.34f, 0.15f, 0.3f);
    for (s = -1; s <= 1; s += 2) {
        Geo_FBox(g, MAT_METAL, f, s * 0.1f, 0.74f, 0.35f, 0.07f, 0.44f, 0.08f);      /* legs */
        Geo_FBox(g, MAT_METAL, f, s * 0.27f, 1.18f, 0.37f, 0.06f, 0.28f, 0.07f);     /* arms */
        Geo_FBox(g, MAT_METAL, f, s * 0.25f, 1.52f, 0.35f, 0.11f, 0.06f, 0.12f);     /* pauldrons */
    }
    Geo_FBox(g, MAT_METAL, f, 0, 1.28f, 0.35f, 0.2f, 0.3f, 0.13f);                   /* breastplate */
    Geo_FBox(g, MAT_METAL, f, 0, 1.76f, 0.35f, 0.12f, 0.15f, 0.13f);                 /* helm */
    Geo_FBox(g, MAT_CLOTH, f, 0, 1.97f, 0.33f, 0.025f, 0.07f, 0.1f);                 /* crest */
    Geo_FBox(g, MAT_WOOD, f, 0.40f, 1.25f, 0.45f, 0.02f, 1.2f, 0.02f);               /* halberd pole */
    Geo_FBox(g, MAT_METAL, f, 0.40f, 2.3f, 0.45f, 0.01f, 0.2f, 0.12f);               /* blade */
}

static void BuildBench(Geo *g, Frame f, int mat, float z)
{
    Geo_FBox(g, mat, f, 0, 0.43f, z, 0.45f, 0.04f, 0.2f);
    Geo_FBox(g, mat, f, -0.35f, 0.2f, z, 0.05f, 0.2f, 0.16f);
    Geo_FBox(g, mat, f, 0.35f, 0.2f, z, 0.05f, 0.2f, 0.16f);
}

/* A picture on the wall (local +z faces the room). */
static void Picture(Geo *g, int mat, Frame f, float cx, float cy, float hw, float hh, float z)
{
    Vector3 p[4] = { Frame_Point(f, cx - hw, cy - hh, z), Frame_Point(f, cx + hw, cy - hh, z),
                     Frame_Point(f, cx + hw, cy + hh, z), Frame_Point(f, cx - hw, cy + hh, z) };
    const Vector2 uv[4] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 } };
    Geo_QuadUV(g, mat, p, uv, false);
}

static void BuildBanner(Geo *g, Frame f, float top)
{
    Geo_FBox(g, MAT_WOOD, f, 0, top, 0.08f, 0.52f, 0.03f, 0.03f);
    Picture(g, MAT_BANNER, f, 0, top - 1.15f, 0.45f, 1.1f, 0.06f);
}

static void BuildPainting(Geo *g, Frame f)
{
    Picture(g, MAT_PAINTING, f, 0, 1.9f, 0.5f, 0.38f, 0.06f);
    Geo_FBox(g, MAT_GOLD, f, 0, 2.31f, 0.05f, 0.56f, 0.04f, 0.03f);
    Geo_FBox(g, MAT_GOLD, f, 0, 1.49f, 0.05f, 0.56f, 0.04f, 0.03f);
    Geo_FBox(g, MAT_GOLD, f, -0.53f, 1.9f, 0.05f, 0.04f, 0.42f, 0.03f);
    Geo_FBox(g, MAT_GOLD, f, 0.53f, 1.9f, 0.05f, 0.04f, 0.42f, 0.03f);
}

static void BuildRug(Geo *g, Frame f)
{
    Vector3 p[4] = { Frame_Point(f, -0.8f, 0.014f, 0.9f), Frame_Point(f, -0.8f, 0.014f, 1.9f),
                     Frame_Point(f, 0.8f, 0.014f, 1.9f), Frame_Point(f, 0.8f, 0.014f, 0.9f) };
    const Vector2 uv[4] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 } };
    Geo_QuadUV(g, MAT_CARPET, p, uv, false);
}

static void BuildRubble(Geo *g, Frame f, int seed)
{
    int k;
    for (k = 0; k < 5; k++) {
        float x = (Hash01(seed, k, 1) - 0.5f) * 0.8f, z = 0.15f + Hash01(seed, k, 2) * 0.5f, s = 0.06f + Hash01(seed, k, 3) * 0.1f;
        Geo_OBox(g, k & 1 ? MAT_WALL_STONE : MAT_TRIM, Frame_Point(f, x, s, z), Hash01(seed, k, 4) * 3.0f, (Vector3){ s * 1.4f, s, s });
    }
}

static void BuildCobweb(Geo *g, const Prop *p)
{
    int a = (int)p->param / 4, b = (int)p->param % 4;
    const World *w = g->w;
    float top = World_CeilingAt(w, p->cx + 0.5f, p->cz + 0.5f);
    const Room *r = w->roomId[p->cz][p->cx] ? &w->rooms[w->roomId[p->cz][p->cx] - 1] : NULL;
    Vector3 c, A, D, B;
    if (r && IsVaultedRoom(w, r)) top = VAULT_SPRING - CORNICE_HEIGHT - 0.02f;
    else top -= CORNICE_HEIGHT + 0.02f;
    c = (Vector3){ p->x, top, p->z };
    A = (Vector3){ c.x - DX[a] * 1.1f, top, c.z };
    D = (Vector3){ c.x, top, c.z - DZ[b] * 1.1f };
    B = (Vector3){ c.x - DX[a] * 0.45f, top - 0.9f, c.z - DZ[b] * 0.45f };
    {
        Vector3 q[4] = { c, A, B, D };
        const Vector2 uv[4] = { { 0, 0 }, { 1, 0 }, { 0.72f, 0.72f }, { 0, 1 } };
        Geo_QuadUV(g, MAT_COBWEB, q, uv, true);
    }
}

static void BuildLanternHang(Geo *g, const Prop *p)
{
    Vector3 c = { p->x, 0, p->z };
    float top = p->param;
    const float rr[4] = { 0.02f, 0.16f, 0.16f, 0.02f }, yy[4] = { 0.0f, 0.06f, 0.10f, 0.22f };
    int k;
    Geo_Box(g, MAT_METAL, (Vector3){ c.x - 0.015f, 2.72f, c.z - 0.015f }, (Vector3){ c.x + 0.015f, top, c.z + 0.015f });  /* chain */
    Geo_Lathe(g, MAT_METAL, (Vector3){ c.x, 2.62f, c.z }, rr, yy, 4, 6, true);                                             /* cap */
    Geo_Box(g, MAT_METAL, (Vector3){ c.x - 0.13f, 2.18f, c.z - 0.13f }, (Vector3){ c.x + 0.13f, 2.24f, c.z + 0.13f });     /* base */
    for (k = 0; k < 4; k++) {
        float sx = (k & 1) ? 0.12f : -0.12f, sz = (k & 2) ? 0.12f : -0.12f;
        Geo_Box(g, MAT_METAL, (Vector3){ c.x + sx - 0.015f, 2.24f, c.z + sz - 0.015f }, (Vector3){ c.x + sx + 0.015f, 2.64f, c.z + sz + 0.015f });
    }
    Candle(g, (Vector3){ c.x, 2.24f, c.z }, 0.14f);
}

static void BuildLanternWall(Geo *g, Frame f)
{
    int k;
    Geo_FBox(g, MAT_METAL, f, 0, 2.45f, 0.03f, 0.06f, 0.12f, 0.03f);
    Geo_FBox(g, MAT_METAL, f, 0, 2.5f, 0.16f, 0.02f, 0.02f, 0.14f);
    Geo_FBox(g, MAT_METAL, f, 0, 2.02f, 0.28f, 0.1f, 0.03f, 0.1f);
    Geo_FBox(g, MAT_METAL, f, 0, 2.42f, 0.28f, 0.11f, 0.03f, 0.11f);
    for (k = 0; k < 4; k++)
        Geo_FBox(g, MAT_METAL, f, (k & 1) ? 0.09f : -0.09f, 2.22f, 0.28f + ((k & 2) ? 0.09f : -0.09f), 0.012f, 0.18f, 0.012f);
    Candle(g, Frame_Point(f, 0, 2.05f, 0.28f), 0.1f);
}

static void BuildTable(Geo *g, Frame f, float len, bool benches)
{
    float h = len * 0.5f;
    int k, s;
    Geo_FBox(g, MAT_WOOD, f, 0, 0.76f, 0, h, 0.04f, benches ? 0.5f : 0.55f);
    for (k = 0; k < 4; k++)
        Geo_FBox(g, MAT_WOOD, f, (k & 1) ? h - 0.12f : -h + 0.12f, 0.36f, (k & 2) ? 0.4f : -0.4f, 0.05f, 0.36f, 0.05f);
    if (benches)
        for (s = -1; s <= 1; s += 2) {
            Geo_FBox(g, MAT_WOOD, f, 0, 0.45f, s * 0.86f, h - 0.15f, 0.03f, 0.16f);
            Geo_FBox(g, MAT_WOOD, f, -h + 0.3f, 0.22f, s * 0.86f, 0.04f, 0.22f, 0.12f);
            Geo_FBox(g, MAT_WOOD, f, h - 0.3f, 0.22f, s * 0.86f, 0.04f, 0.22f, 0.12f);
        }
    for (k = 0; k < (benches ? 3 : 2); k++) {
        float x = (k - (benches ? 1.0f : 0.5f)) * (benches ? 1.3f : 1.2f);
        Geo_FBox(g, MAT_METAL, f, x, 0.81f, 0, 0.06f, 0.01f, 0.06f);
        Candle(g, Frame_Point(f, x, 0.82f, 0), 0.17f);
    }
    if (!benches) {                                    /* open book and inkwell */
        Geo_FBox(g, MAT_BONE, f, -0.2f, 0.81f, 0.1f, 0.22f, 0.01f, 0.16f);
        Geo_FBox(g, MAT_METAL, f, 0.3f, 0.83f, -0.2f, 0.04f, 0.04f, 0.04f);
    }
}

static void BuildChandelier(Geo *g, const Prop *p)
{
    float top = p->param, y = top - 1.9f;
    int k;
    for (k = 0; k < 12; k++) {
        float a = k * PI / 6.0f;
        Geo_OBox(g, MAT_METAL, (Vector3){ p->x + cosf(a) * 0.95f, y, p->z + sinf(a) * 0.95f }, -a, (Vector3){ 0.03f, 0.03f, 0.27f });
    }
    for (k = 0; k < 8; k++) {
        float a = k * PI / 4.0f;
        Candle(g, (Vector3){ p->x + cosf(a) * 0.95f, y + 0.03f, p->z + sinf(a) * 0.95f }, 0.16f);
    }
    for (k = 0; k < 3; k++) {                            /* chains up to one hook */
        float a = k * 2.0f * PI / 3.0f;
        Vector3 lo = { p->x + cosf(a) * 0.9f, y, p->z + sinf(a) * 0.9f }, hi = { p->x, y + 1.2f, p->z };
        Vector3 mid = Vector3Lerp(lo, hi, 0.5f);
        float len = Vector3Distance(lo, hi);
        /* approximate the slanted chain with three short vertical links along the line */
        int s;
        for (s = 0; s < 3; s++) {
            Vector3 q = Vector3Lerp(lo, hi, (s + 0.5f) / 3.0f);
            Geo_Box(g, MAT_METAL, (Vector3){ q.x - 0.012f, q.y - len / 6.0f, q.z - 0.012f }, (Vector3){ q.x + 0.012f, q.y + len / 6.0f, q.z + 0.012f });
        }
        (void)mid;
    }
    Geo_Box(g, MAT_METAL, (Vector3){ p->x - 0.015f, y + 1.2f, p->z - 0.015f }, (Vector3){ p->x + 0.015f, top, p->z + 0.015f });
}

static void BuildBed(Geo *g, Frame f)
{
    int k;
    Geo_FBox(g, MAT_WOOD, f, 0, 0.28f, 1.1f, 0.72f, 0.14f, 1.02f);                   /* frame */
    Geo_FBox(g, MAT_BONE, f, 0, 0.48f, 1.1f, 0.66f, 0.07f, 0.98f);                   /* sheets */
    Geo_FBox(g, MAT_CLOTH, f, 0, 0.57f, 1.38f, 0.68f, 0.03f, 0.72f);                 /* blanket */
    Geo_FBox(g, MAT_BONE, f, 0, 0.6f, 0.32f, 0.5f, 0.06f, 0.14f);                    /* pillow */
    Geo_FBox(g, MAT_WOOD, f, 0, 0.95f, 0.1f, 0.72f, 0.5f, 0.04f);                    /* headboard */
    for (k = 0; k < 4; k++) {
        float x = (k & 1) ? 0.7f : -0.7f, z = (k & 2) ? 2.1f : 0.1f;
        Geo_FBox(g, MAT_WOOD, f, x, 1.15f, z, 0.05f, 1.15f, 0.05f);                  /* posts */
        Geo_FBox(g, MAT_CLOTH, f, x * 0.96f, 1.45f, z + ((k & 2) ? -0.16f : 0.16f), 0.035f, 0.85f, 0.13f);   /* curtains */
    }
    Geo_FBox(g, MAT_CLOTH, f, 0, 2.32f, 1.1f, 0.76f, 0.04f, 1.06f);                  /* canopy */
}

static void BuildTrunk(Geo *g, Frame f)
{
    Geo_FBox(g, MAT_WOOD, f, 0, 0.26f, 0.35f, 0.4f, 0.24f, 0.26f);
    Geo_FBox(g, MAT_METAL, f, -0.25f, 0.27f, 0.35f, 0.04f, 0.26f, 0.28f);
    Geo_FBox(g, MAT_METAL, f, 0.25f, 0.27f, 0.35f, 0.04f, 0.26f, 0.28f);
    Geo_FBox(g, MAT_GOLD, f, 0, 0.38f, 0.62f, 0.05f, 0.06f, 0.015f);
}

static void BuildFireplace(Geo *g, Frame f)
{
    Geo_FBox(g, MAT_TRIM, f, -0.85f, 0.8f, 0.3f, 0.2f, 0.8f, 0.3f);                   /* jambs */
    Geo_FBox(g, MAT_TRIM, f, 0.85f, 0.8f, 0.3f, 0.2f, 0.8f, 0.3f);
    Geo_FBox(g, MAT_TRIM, f, 0, 1.7f, 0.36f, 1.18f, 0.1f, 0.4f);                     /* mantel */
    Geo_FBox(g, MAT_WALL_STONE, f, 0, 2.7f, 0.22f, 0.95f, 0.9f, 0.22f);              /* chimney breast */
    Geo_FBox(g, MAT_METAL, f, 0, 0.8f, 0.04f, 0.66f, 0.8f, 0.04f);                   /* sooty back */
    Geo_FBox(g, MAT_TRIM, f, 0, 0.04f, 0.5f, 0.95f, 0.04f, 0.32f);                   /* hearth */
    Geo_OBox(g, MAT_WOOD, Frame_Point(f, -0.1f, 0.12f, 0.32f), f.yaw + 0.3f, (Vector3){ 0.4f, 0.06f, 0.06f });   /* logs */
    Geo_OBox(g, MAT_WOOD, Frame_Point(f, 0.1f, 0.12f, 0.36f), f.yaw - 0.4f, (Vector3){ 0.38f, 0.06f, 0.06f });
}

static void BuildBookcase(Geo *g, Frame f)
{
    Geo_FBox(g, MAT_WOOD, f, 0, 1.3f, 0.2f, 0.5f, 1.3f, 0.2f);
    Picture(g, MAT_BOOKS, f, 0, 1.28f, 0.45f, 1.2f, 0.41f);
    Geo_FBox(g, MAT_WOOD, f, 0, 2.66f, 0.22f, 0.52f, 0.06f, 0.24f);
}

static void BuildDesk(Geo *g, Frame f)
{
    Geo_FBox(g, MAT_WOOD, f, -0.65f, 0.4f, 0, 0.32f, 0.4f, 0.5f);                    /* pedestals */
    Geo_FBox(g, MAT_WOOD, f, 0.65f, 0.4f, 0, 0.32f, 0.4f, 0.5f);
    Geo_FBox(g, MAT_GOLD, f, -0.65f, 0.45f, 0.51f, 0.26f, 0.3f, 0.01f);             /* carved panels */
    Geo_FBox(g, MAT_GOLD, f, 0.65f, 0.45f, 0.51f, 0.26f, 0.3f, 0.01f);
    Geo_FBox(g, MAT_WOOD, f, 0, 0.83f, 0, 1.02f, 0.04f, 0.6f);                      /* top */
    Geo_FBox(g, MAT_BONE, f, -0.3f, 0.88f, 0.1f, 0.25f, 0.01f, 0.18f);              /* open book */
    Geo_FBox(g, MAT_CLOTH, f, 0.2f, 0.92f, -0.25f, 0.12f, 0.05f, 0.09f);            /* closed books */
    Geo_FBox(g, MAT_WOOD, f, 0.2f, 1.0f, -0.25f, 0.11f, 0.03f, 0.08f);
    Geo_FBox(g, MAT_METAL, f, 0.7f, 0.88f, 0.2f, 0.05f, 0.01f, 0.05f);
    Candle(g, Frame_Point(f, 0.7f, 0.88f, 0.2f), 0.14f);
}

static void BuildGlobe(Geo *g, Frame f)
{
    const float sr[3] = { 0.18f, 0.04f, 0.03f }, sy[3] = { 0.0f, 0.06f, 0.75f };
    float r[9], y[9];
    int k;
    Geo_Lathe(g, MAT_WOOD, f.o, sr, sy, 3, 6, true);
    for (k = 0; k < 9; k++) { float a = PI * k / 8.0f; r[k] = 0.28f * sinf(a) + 0.001f; y[k] = 0.75f + 0.28f - 0.28f * cosf(a); }
    Geo_Lathe(g, MAT_GOLD, f.o, r, y, 9, 8, false);
    Geo_FBox(g, MAT_METAL, f, 0, 1.03f, 0, 0.32f, 0.015f, 0.015f);
}

static void BuildStair(Geo *g, const Prop *p)
{
    int k, steps = 16;
    float top = fminf(p->param - 0.4f, 4.8f);
    const float pr[2] = { 0.12f, 0.12f }, py[2] = { 0.0f, 0.0f };
    float py2[2] = { 0.0f, top + 0.6f };
    (void)py;
    Geo_Lathe(g, MAT_METAL, (Vector3){ p->x, 0, p->z }, pr, py2, 2, 8, true);
    for (k = 0; k < steps; k++) {
        float a = k * 0.42f, y = (k + 1) * top / steps;
        Vector3 c = { p->x + cosf(a) * 0.45f, y, p->z + sinf(a) * 0.45f };
        Geo_OBox(g, MAT_WOOD, c, -a + PI * 0.5f, (Vector3){ 0.13f, 0.035f, 0.38f });
        Geo_Box(g, MAT_METAL, (Vector3){ p->x + cosf(a) * 0.8f - 0.012f, y, p->z + sinf(a) * 0.8f - 0.012f },
                (Vector3){ p->x + cosf(a) * 0.8f + 0.012f, y + 0.85f, p->z + sinf(a) * 0.8f + 0.012f });   /* railing posts */
    }
}

static void BuildGallery(Geo *g, Frame f)
{
    Geo_FBox(g, MAT_WOOD, f, 0, 3.25f, 0.4f, 0.5f, 0.06f, 0.4f);                     /* walkway */
    Geo_FBox(g, MAT_WOOD, f, 0, 3.12f, 0.76f, 0.5f, 0.08f, 0.04f);                  /* fascia */
    Geo_FBox(g, MAT_WOOD, f, 0, 3.86f, 0.76f, 0.5f, 0.025f, 0.025f);                /* rail */
    Geo_FBox(g, MAT_WOOD, f, 0.45f, 3.58f, 0.76f, 0.02f, 0.28f, 0.02f);             /* baluster */
    Geo_FBox(g, MAT_WOOD, f, 0, 2.7f, 0.06f, 0.06f, 0.5f, 0.06f);                   /* bracket */
}

static void BuildJarShelf(Geo *g, Frame f, int seed)
{
    int s, k;
    Geo_FBox(g, MAT_WOOD, f, -0.47f, 1.05f, 0.22f, 0.03f, 1.05f, 0.2f);
    Geo_FBox(g, MAT_WOOD, f, 0.47f, 1.05f, 0.22f, 0.03f, 1.05f, 0.2f);
    for (s = 0; s < 4; s++) {
        float y = 0.25f + s * 0.55f;
        Geo_FBox(g, MAT_WOOD, f, 0, y, 0.22f, 0.47f, 0.02f, 0.2f);
        for (k = 0; k < 3; k++) {
            float r0 = 0.05f + Hash01(seed, s * 3 + k, 1) * 0.04f, hgt = 0.12f + Hash01(seed, s * 3 + k, 2) * 0.16f;
            const float rr[5] = { r0, r0 * 1.1f, r0, r0 * 0.4f, r0 * 0.45f };
            const float yy[5] = { 0.0f, hgt * 0.3f, hgt * 0.75f, hgt * 0.85f, hgt };
            Geo_Lathe(g, MAT_GLASS, Frame_Point(f, -0.3f + k * 0.3f, y + 0.02f, 0.22f), rr, yy, 5, 6, true);
        }
    }
}

static void BuildCauldron(Geo *g, Frame f)
{
    const float r[6] = { 0.22f, 0.4f, 0.46f, 0.44f, 0.38f, 0.40f }, y[6] = { 0.12f, 0.2f, 0.42f, 0.62f, 0.72f, 0.76f };
    const float pr[1] = { 0.37f }, py[1] = { 0.68f };
    int k;
    Geo_Lathe(g, MAT_METAL, f.o, r, y, 6, 8, false);
    Geo_Lathe(g, MAT_POTION, f.o, pr, py, 1, 8, true);
    for (k = 0; k < 3; k++) {
        float a = k * 2.0f * PI / 3.0f;
        Geo_Box(g, MAT_METAL, (Vector3){ f.o.x + cosf(a) * 0.3f - 0.04f, 0, f.o.z + sinf(a) * 0.3f - 0.04f },
                (Vector3){ f.o.x + cosf(a) * 0.3f + 0.04f, 0.2f, f.o.z + sinf(a) * 0.3f + 0.04f });
    }
}

static void BuildWorkbench(Geo *g, Frame f)
{
    const float rr[4] = { 0.06f, 0.07f, 0.03f, 0.03f }, yy[4] = { 0.0f, 0.12f, 0.18f, 0.24f };
    int k;
    Geo_FBox(g, MAT_WOOD, f, 0, 0.84f, 0.4f, 0.48f, 0.04f, 0.36f);
    for (k = 0; k < 4; k++) Geo_FBox(g, MAT_WOOD, f, (k & 1) ? 0.42f : -0.42f, 0.4f, (k & 2) ? 0.7f : 0.1f, 0.04f, 0.4f, 0.04f);
    Geo_Lathe(g, MAT_GLASS, Frame_Point(f, -0.2f, 0.88f, 0.35f), rr, yy, 4, 6, true);
    Geo_Lathe(g, MAT_GLASS, Frame_Point(f, 0.15f, 0.88f, 0.5f), rr, yy, 4, 6, true);
    Geo_FBox(g, MAT_BONE, f, 0.25f, 0.9f, 0.25f, 0.06f, 0.05f, 0.05f);             /* a skull */
}

static void BuildStool(Geo *g, Frame f)
{
    const float r[2] = { 0.2f, 0.2f }, y[2] = { 0.46f, 0.52f };
    int k;
    Geo_Lathe(g, MAT_WOOD, f.o, r, y, 2, 6, true);
    for (k = 0; k < 3; k++) {
        float a = k * 2.0f * PI / 3.0f;
        Geo_Box(g, MAT_WOOD, (Vector3){ f.o.x + cosf(a) * 0.13f - 0.025f, 0, f.o.z + sinf(a) * 0.13f - 0.025f },
                (Vector3){ f.o.x + cosf(a) * 0.13f + 0.025f, 0.46f, f.o.z + sinf(a) * 0.13f + 0.025f });
    }
}

static void BuildCoffin(Geo *g, Frame f)
{
    Geo_FBox(g, MAT_TRIM, f, 0, 0.3f, 0, 0.45f, 0.3f, 1.0f);
    Geo_FBox(g, MAT_TRIM, f, 0, 0.64f, 0, 0.5f, 0.05f, 1.04f);
    Geo_FBox(g, MAT_WALL_STONE, f, 0, 0.7f, 0.25f, 0.05f, 0.015f, 0.42f);           /* carved cross */
    Geo_FBox(g, MAT_WALL_STONE, f, 0, 0.7f, 0.42f, 0.22f, 0.015f, 0.05f);
}

static void BuildSkullNiche(Geo *g, Frame f)
{
    Picture(g, MAT_METAL, f, 0, 1.25f, 0.32f, 0.26f, 0.02f);                         /* dark recess */
    Geo_FBox(g, MAT_TRIM, f, 0, 0.97f, 0.06f, 0.38f, 0.03f, 0.07f);                  /* sill */
    Geo_FBox(g, MAT_TRIM, f, 0, 1.55f, 0.05f, 0.38f, 0.05f, 0.06f);
    Geo_FBox(g, MAT_BONE, f, 0, 1.1f, 0.1f, 0.11f, 0.1f, 0.1f);                     /* skull */
    Geo_FBox(g, MAT_METAL, f, -0.045f, 1.13f, 0.2f, 0.03f, 0.025f, 0.005f);         /* sockets */
    Geo_FBox(g, MAT_METAL, f, 0.045f, 1.13f, 0.2f, 0.03f, 0.025f, 0.005f);
    Geo_FBox(g, MAT_BONE, f, -0.2f, 1.03f, 0.12f, 0.08f, 0.03f, 0.03f);
}

static void BuildMirror(Geo *g, Frame f)
{
    const float z = 0.5f;
    Geo_FBox(g, MAT_WOOD, f, 0, 0.06f, z, 0.6f, 0.06f, 0.26f);                       /* stand */
    Geo_FBox(g, MAT_GOLD, f, -0.47f, 1.25f, z, 0.05f, 1.15f, 0.05f);                /* posts */
    Geo_FBox(g, MAT_GOLD, f, 0.47f, 1.25f, z, 0.05f, 1.15f, 0.05f);
    Geo_FBox(g, MAT_GOLD, f, 0, 0.22f, z, 0.5f, 0.05f, 0.05f);
    Geo_FBox(g, MAT_GOLD, f, -0.47f, 2.55f, z, 0.03f, 0.2f, 0.03f);                 /* finials */
    Geo_FBox(g, MAT_GOLD, f, 0.47f, 2.55f, z, 0.03f, 0.2f, 0.03f);
    Picture(g, MAT_MIRROR, f, 0, 1.3f, 0.42f, 1.03f, z + 0.01f);                    /* glass */
    {
        /* pointed top: glass gable + gold edges */
        Vector3 a = Frame_Point(f, -0.42f, 2.33f, z + 0.01f), b = Frame_Point(f, 0.42f, 2.33f, z + 0.01f);
        Vector3 t = Frame_Point(f, 0, 2.85f, z + 0.01f);
        Vector3 q[4] = { a, b, t, t };
        const Vector2 uv[4] = { { 0, 1 }, { 1, 1 }, { 0.5f, 0 }, { 0.5f, 0 } };
        Geo_QuadUV(g, MAT_MIRROR, q, uv, false);
        Geo_Quad2(g, MAT_GOLD, Frame_Point(f, -0.5f, 2.3f, z), Frame_Point(f, -0.42f, 2.3f, z), Frame_Point(f, 0, 2.86f, z), Frame_Point(f, 0, 2.95f, z));
        Geo_Quad2(g, MAT_GOLD, Frame_Point(f, 0.42f, 2.3f, z), Frame_Point(f, 0.5f, 2.3f, z), Frame_Point(f, 0, 2.95f, z), Frame_Point(f, 0, 2.86f, z));
        Geo_FBox(g, MAT_GOLD, f, 0, 3.0f, z, 0.03f, 0.12f, 0.03f);
    }
    Geo_FBox(g, MAT_WOOD, f, 0, 1.3f, z - 0.03f, 0.43f, 1.03f, 0.01f);               /* back */
}

static void BuildPew(Geo *g, Frame f, float broken)
{
    float len = broken > 0.6f ? 0.45f : 0.75f, off = broken > 0.6f ? -0.3f : 0.0f;
    Geo_FBox(g, MAT_WOOD, f, off, 0.45f, 0.1f, len, 0.03f, 0.22f);
    Geo_FBox(g, MAT_WOOD, f, off, 0.82f, -0.12f, len, 0.35f, 0.03f);
    Geo_FBox(g, MAT_WOOD, f, off - len + 0.05f, 0.42f, 0.0f, 0.04f, 0.42f, 0.24f);
    Geo_FBox(g, MAT_WOOD, f, off + len - 0.05f, 0.42f, 0.0f, 0.04f, 0.42f, 0.24f);
    if (broken > 0.6f)                                                                 /* a fallen plank */
        Geo_OBox(g, MAT_WOOD, Frame_Point(f, 0.55f, 0.04f, 0.4f), f.yaw + 0.7f, (Vector3){ 0.5f, 0.03f, 0.1f });
}

static void BuildBell(Geo *g, const Prop *p)
{
    const float r[8] = { 1.25f, 1.2f, 1.0f, 0.78f, 0.68f, 0.62f, 0.45f, 0.12f };
    const float y[8] = { 0.0f, 0.15f, 0.5f, 1.0f, 1.4f, 1.8f, 2.0f, 2.1f };
    float ri[8], yi[8];
    float base = p->param - 3.2f;
    int k;
    Vector3 c = { p->x, base, p->z };
    Geo_Lathe(g, MAT_GOLD, c, r, y, 8, 12, true);
    for (k = 0; k < 8; k++) { ri[7 - k] = r[k] * 0.9f; yi[7 - k] = y[k] - 0.05f; }        /* inside, facing in */
    Geo_Lathe(g, MAT_METAL, c, ri, yi, 8, 12, false);
    {
        const float cr[4] = { 0.04f, 0.04f, 0.16f, 0.12f }, cy[4] = { 1.9f, 0.6f, 0.45f, 0.2f };
        Geo_Lathe(g, MAT_METAL, c, cr, cy, 4, 6, false);                                 /* clapper */
    }
    Geo_Box(g, MAT_WOOD, (Vector3){ p->x - 1.6f, base + 2.1f, p->z - 0.15f }, (Vector3){ p->x + 1.6f, base + 2.45f, p->z + 0.15f });   /* yoke */
    Geo_Box(g, MAT_METAL, (Vector3){ p->x - 0.03f, base + 2.45f, p->z - 0.03f }, (Vector3){ p->x + 0.03f, p->param, p->z + 0.03f });
    Geo_Box(g, MAT_WOOD, (Vector3){ p->x + 1.3f - 0.02f, 1.1f, p->z - 0.02f }, (Vector3){ p->x + 1.3f + 0.02f, base + 2.1f, p->z + 0.02f });  /* bell rope */
}

static void BuildRope(Geo *g, const Prop *p)
{
    Geo_Box(g, MAT_WOOD, (Vector3){ p->x - 0.025f, 1.3f, p->z - 0.025f }, (Vector3){ p->x + 0.025f, p->param, p->z + 0.025f });
    Geo_Box(g, MAT_WOOD, (Vector3){ p->x - 0.05f, 1.15f, p->z - 0.05f }, (Vector3){ p->x + 0.05f, 1.3f, p->z + 0.05f });
}

static void BuildPortcullis(Geo *g, Frame f)
{
    int k;
    for (k = -4; k <= 4; k++) Geo_FBox(g, MAT_METAL, f, k * 0.22f, 1.5f, 0.06f, 0.025f, 1.5f, 0.025f);
    for (k = 0; k < 6; k++) Geo_FBox(g, MAT_METAL, f, 0, 0.3f + k * 0.5f, 0.09f, 0.95f, 0.025f, 0.02f);
    Geo_FBox(g, MAT_TRIM, f, -1.1f, 1.6f, 0.08f, 0.12f, 1.6f, 0.08f);               /* gate posts */
    Geo_FBox(g, MAT_TRIM, f, 1.1f, 1.6f, 0.08f, 0.12f, 1.6f, 0.08f);
    Geo_FBox(g, MAT_TRIM, f, 0, 3.3f, 0.08f, 1.22f, 0.14f, 0.09f);
}

static void BuildCandles(Geo *g, Frame f)
{
    int k;
    for (k = 0; k < 3; k++) Candle(g, Frame_Point(f, -0.2f + k * 0.2f, 0.0f, 0.25f + (k & 1) * 0.1f), 0.3f + k * 0.12f);
}

/* Banner/painting height on a wall: below the cornice, never higher than 4. */
static float DecorTop(const World *w, const Prop *p)
{
    float top = World_CeilingAt(w, p->cx + 0.5f, p->cz + 0.5f);
    const Room *r = w->roomId[p->cz][p->cx] ? &w->rooms[w->roomId[p->cz][p->cx] - 1] : NULL;
    if (r && IsVaultedRoom(w, r)) top = VAULT_SPRING;
    return fminf(top - CORNICE_HEIGHT - 0.3f, 4.0f);
}

void Props_Build(Geo *g)
{
    int i;
    for (i = 0; i < g->w->propCount; i++) {
        const Prop *p = &g->w->props[i];
        Frame f = { { p->x, 0.0f, p->z }, p->yaw };
        switch (p->type) {
        case P_CANDELABRA:   BuildCandelabra(g, f); break;
        case P_ARMOR:        BuildArmor(g, f); break;
        case P_STONE_BENCH:  BuildBench(g, f, MAT_TRIM, 0.3f); break;
        case P_BENCH:        BuildBench(g, f, MAT_WOOD, 0.25f); break;
        case P_BANNER:       BuildBanner(g, f, DecorTop(g->w, p)); break;
        case P_PAINTING:     BuildPainting(g, f); break;
        case P_RUG:          BuildRug(g, f); break;
        case P_RUBBLE:       BuildRubble(g, f, i); break;
        case P_COBWEB:       BuildCobweb(g, p); break;
        case P_LANTERN_HANG: BuildLanternHang(g, p); break;
        case P_LANTERN_WALL: BuildLanternWall(g, f); break;
        case P_TABLE:        BuildTable(g, f, p->param, true); break;
        case P_DESK:         if (p->param > 1.9f) BuildDesk(g, f); else BuildTable(g, f, p->param, false); break;
        case P_CHANDELIER:   BuildChandelier(g, p); break;
        case P_BED:          BuildBed(g, f); break;
        case P_TRUNK:        BuildTrunk(g, f); break;
        case P_FIREPLACE:    BuildFireplace(g, f); break;
        case P_BOOKCASE:     BuildBookcase(g, f); break;
        case P_GLOBE:        BuildGlobe(g, f); break;
        case P_STAIR:        BuildStair(g, p); break;
        case P_GALLERY:      BuildGallery(g, f); break;
        case P_JAR_SHELF:    BuildJarShelf(g, f, i); break;
        case P_CAULDRON:     BuildCauldron(g, f); break;
        case P_WORKBENCH:    BuildWorkbench(g, f); break;
        case P_STOOL:        BuildStool(g, f); break;
        case P_COFFIN:       BuildCoffin(g, f); break;
        case P_SKULL_NICHE:  BuildSkullNiche(g, f); break;
        case P_MIRROR:       BuildMirror(g, f); break;
        case P_PEW:          BuildPew(g, f, p->param); break;
        case P_BELL:         BuildBell(g, p); break;
        case P_ROPE:         BuildRope(g, p); break;
        case P_PORTCULLIS:   BuildPortcullis(g, f); break;
        case P_CANDLES:      BuildCandles(g, f); break;
        default: break;
        }
    }
}
