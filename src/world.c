/* world.c - loads a wing map (text grid), validates it, classifies rooms and corridors, and
 * answers collision / line-of-sight / light questions. The 3D architecture built from the grid
 * lives in architecture.c. See CLAUDE.md section 7.1 for the map format. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "textures.h"
#include "raymath.h"

/* ------------------------------------------------------------ cell types */

static bool IsWallChar(char c)  { return c == '#' || c == 'W' || c == 'B' || c == 'P' || c == 'T'; }
static bool IsFloorChar(char c) { return c == '.' || c == '=' || c == ','; }
static bool IsKnownChar(char c) { return IsWallChar(c) || IsFloorChar(c) || (c && strchr("x@CEsgwQOa", c)); }

static int FloorMat(char c)
{
    if (c == '=') return MAT_CARPET;
    if (c == ',') return MAT_WOOD;
    return MAT_FLOOR;
}

static const int DX[4] = { 1, -1, 0, 0 };
static const int DZ[4] = { 0, 0, 1, -1 };

/* ------------------------------------------------------------ queries */

static bool InBounds(const World *w, int x, int z) { return x >= 0 && z >= 0 && x < w->w && z < w->h; }

bool World_IsSolid(const World *w, int x, int z)
{
    char c;
    if (!InBounds(w, x, z)) return true;
    c = w->grid[z][x];
    if (IsWallChar(c) || c == 'C') return true;
    if (c == 'E') return !w->exitOpen;
    return false;
}

bool World_IsSolidAt(const World *w, float x, float z)
{
    return World_IsSolid(w, (int)floorf(x), (int)floorf(z));
}

bool World_IsWallCell(const World *w, int x, int z)
{
    char c;
    if (!InBounds(w, x, z)) return true;
    c = w->grid[z][x];
    return IsWallChar(c) || (c == 'E' && !w->exitOpen);
}

bool World_BoxBlocked(const World *w, float cx, float cz, float size)
{
    float h = size * 0.5f;
    int x0 = (int)floorf(cx - h), x1 = (int)floorf(cx + h - 0.001f);
    int z0 = (int)floorf(cz - h), z1 = (int)floorf(cz + h - 0.001f);
    int x, z;
    for (z = z0; z <= z1; z++)
        for (x = x0; x <= x1; x++)
            if (World_IsSolid(w, x, z)) return true;
    return false;
}

void World_Move(const World *w, Vector3 *pos, float size, Vector3 delta)
{
    /* sub-steps so fast moves (dash) still stop flush against walls */
    float len = sqrtf(delta.x * delta.x + delta.z * delta.z);
    int steps = (int)(len / 0.05f) + 1, i;
    float sx = delta.x / steps, sz = delta.z / steps;
    for (i = 0; i < steps; i++) {
        pos->x += sx;
        if (World_BoxBlocked(w, pos->x, pos->z, size)) pos->x -= sx;
        pos->z += sz;
        if (World_BoxBlocked(w, pos->x, pos->z, size)) pos->z -= sz;
    }
}

bool World_LineOfSight(const World *w, Vector3 a, Vector3 b)
{
    float dx = b.x - a.x, dz = b.z - a.z;
    int steps = (int)(sqrtf(dx * dx + dz * dz) / 0.1f) + 1, i;
    for (i = 1; i < steps; i++) {
        float t = (float)i / (float)steps;
        if (World_IsWallCell(w, (int)floorf(a.x + dx * t), (int)floorf(a.z + dz * t))) return false;
    }
    return true;
}

float World_CeilingAt(const World *w, float x, float z)
{
    int cx = (int)floorf(x), cz = (int)floorf(z);
    if (!InBounds(w, cx, cz) || w->ceiling[cz][cx] <= 0.0f) return CORRIDOR_HEIGHT;
    return w->ceiling[cz][cx];
}

/* ------------------------------------------------------------ rooms and corridors */

/* Open for architecture: everything except walls (pillars stand inside rooms, so they count). */
static bool IsOpenForArea(char c) { return !IsWallChar(c) || c == 'P'; }

/* Room cells lie inside some fully open ROOM_MIN_OPEN x ROOM_MIN_OPEN square; all other open
 * cells are corridor cells. Connected room cells form one Room. */
/* Islands: small groups of wall cells standing free inside a room (bookcases, piers) that do not
 * touch the outer walls. Rooms extend around them. */
static void FindIslands(World *w)
{
    static short qx[WORLD_MAX_W * WORLD_MAX_H], qz[WORLD_MAX_W * WORLD_MAX_H];
    static unsigned char seen[WORLD_MAX_H][WORLD_MAX_W];
    int x, z, d;
    memset(seen, 0, sizeof(seen));
    memset(w->island, 0, sizeof(w->island));
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            int head = 0, tail = 0, k;
            bool border = false;
            if (seen[z][x] || !IsWallChar(w->grid[z][x]) || w->grid[z][x] == 'P') continue;
            seen[z][x] = 1;
            qx[tail] = (short)x; qz[tail] = (short)z; tail++;
            while (head < tail) {
                int cx = qx[head], cz = qz[head];
                head++;
                if (cx == 0 || cz == 0 || cx == w->w - 1 || cz == w->h - 1) border = true;
                for (d = 0; d < 4; d++) {
                    int nx = cx + DX[d], nz = cz + DZ[d];
                    if (!InBounds(w, nx, nz) || seen[nz][nx] || !IsWallChar(w->grid[nz][nx]) || w->grid[nz][nx] == 'P') continue;
                    seen[nz][nx] = 1;
                    qx[tail] = (short)nx; qz[tail] = (short)nz; tail++;
                }
            }
            if (border || tail > ISLAND_MAX_CELLS) continue;
            for (k = 0; k < tail; k++) w->island[qz[k]][qx[k]] = 1;
        }
    }
}

static void ClassifyAreas(World *w)
{
    static short qx[WORLD_MAX_W * WORLD_MAX_H], qz[WORLD_MAX_W * WORLD_MAX_H];
    const int n = ROOM_MIN_OPEN;
    int x, z, i, j, d;

    FindIslands(w);
    memset(w->area, AREA_SOLID, sizeof(w->area));
    memset(w->roomId, 0, sizeof(w->roomId));
    w->roomCount = 0;
    for (z = 0; z < w->h; z++)
        for (x = 0; x < w->w; x++)
            if (IsOpenForArea(w->grid[z][x]) && w->grid[z][x] != 'E') w->area[z][x] = AREA_CORRIDOR;
    for (z = 0; z + n <= w->h; z++) {
        for (x = 0; x + n <= w->w; x++) {
            bool open = true;
            for (j = 0; j < n && open; j++)
                for (i = 0; i < n && open; i++)
                    if (w->area[z + j][x + i] == AREA_SOLID && !w->island[z + j][x + i]) open = false;
            if (!open) continue;
            for (j = 0; j < n; j++)
                for (i = 0; i < n; i++)
                    if (w->area[z + j][x + i] != AREA_SOLID) w->area[z + j][x + i] = AREA_ROOM;
        }
    }
    for (z = 0; z < w->h; z++)                     /* exit doorways are short corridor passages */
        for (x = 0; x < w->w; x++)
            if (w->grid[z][x] == 'E') w->area[z][x] = AREA_CORRIDOR;
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            int head = 0, tail = 0;
            Room *r;
            if (w->area[z][x] != AREA_ROOM || w->roomId[z][x] || w->roomCount >= MAX_ROOMS) continue;
            r = &w->rooms[w->roomCount++];
            *r = (Room){ x, z, x, z, 0 };
            w->roomId[z][x] = (unsigned char)w->roomCount;
            qx[tail] = (short)x; qz[tail] = (short)z; tail++;
            while (head < tail) {
                int cx = qx[head], cz = qz[head];
                head++;
                r->cells++;
                if (cx < r->x0) r->x0 = cx;
                if (cx > r->x1) r->x1 = cx;
                if (cz < r->z0) r->z0 = cz;
                if (cz > r->z1) r->z1 = cz;
                for (d = 0; d < 4; d++) {
                    int nx = cx + DX[d], nz = cz + DZ[d];
                    if (!InBounds(w, nx, nz) || w->area[nz][nx] != AREA_ROOM || w->roomId[nz][nx]) continue;
                    w->roomId[nz][nx] = (unsigned char)w->roomCount;
                    qx[tail] = (short)nx; qz[tail] = (short)nz; tail++;
                }
            }
        }
    }
}

/* ------------------------------------------------------------ loading */

static int Report(const char *file, int line, int col, const char *fmt, ...)
{
    va_list ap;
    printf("%s:%d:%d: error: ", file, line, col);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    return 1;
}

/* Direction (as a yaw) with the longest run of open floor from cell (x,z). */
static float OpenYaw(const World *w, int x, int z)
{
    static const float yaws[4] = { PI * 0.5f, -PI * 0.5f, 0.0f, PI };   /* +X, -X, +Z, -Z */
    int best = 3, bestLen = -1, d;
    for (d = 0; d < 4; d++) {
        int len = 0;
        while (len < 20 && !World_IsSolid(w, x + DX[d] * (len + 1), z + DZ[d] * (len + 1))) len++;
        if (len > bestLen) { bestLen = len; best = d; }
    }
    return yaws[best];
}

/* Special cells (@ C E s g w Q x) take the floor type of the nearest plain floor cell. */
static int NearestFloorMat(const World *w, int sx, int sz)
{
    static short qx[WORLD_MAX_W * WORLD_MAX_H], qz[WORLD_MAX_W * WORLD_MAX_H];
    static unsigned char seen[WORLD_MAX_H][WORLD_MAX_W];
    int head = 0, tail = 0, d;
    memset(seen, 0, sizeof(seen));
    qx[tail] = (short)sx; qz[tail] = (short)sz; tail++;
    seen[sz][sx] = 1;
    while (head < tail) {
        int x = qx[head], z = qz[head];
        head++;
        if (IsFloorChar(w->grid[z][x])) return FloorMat(w->grid[z][x]);
        for (d = 0; d < 4; d++) {
            int nx = x + DX[d], nz = z + DZ[d];
            if (!InBounds(w, nx, nz) || seen[nz][nx] || IsWallChar(w->grid[nz][nx])) continue;
            seen[nz][nx] = 1;
            qx[tail] = (short)nx; qz[tail] = (short)nz; tail++;
        }
    }
    return MAT_FLOOR;
}

bool World_Load(World *w, const char *path, int expectedChests, bool needExit)
{
    static short qx[WORLD_MAX_W * WORLD_MAX_H], qz[WORLD_MAX_W * WORLD_MAX_H];
    static unsigned char reach[WORLD_MAX_H][WORLD_MAX_W];
    const char *file = GetFileName(path);
    char *text, *p;
    int errors = 0, line = 0, starts = 0, x, z, d, head = 0, tail = 0, unreachable = 0;

    memset(w, 0, sizeof(*w));
    text = LoadFileText(path);
    if (!text) {
        printf("%s: error: cannot open wing file\n", path);
        return false;
    }

    /* ---- split into lines: first line is the name, the rest is the grid ---- */
    p = text;
    while (*p) {
        char *end = p;
        int len;
        while (*end && *end != '\n') end++;
        len = (int)(end - p);
        if (len > 0 && p[len - 1] == '\r') len--;
        line++;
        if (line == 1) {
            if (len > (int)sizeof(w->name) - 1) len = (int)sizeof(w->name) - 1;
            memcpy(w->name, p, (size_t)len);
        } else if (len == 0) {
            /* blank lines are only allowed after the grid */
        } else if (w->h > 0 && line - 2 != w->h) {
            errors += Report(file, line, 1, "blank line inside the grid");
            break;
        } else if (w->h >= WORLD_MAX_H) {
            errors += Report(file, line, 1, "map is taller than %d rows", WORLD_MAX_H);
            break;
        } else if (w->h > 0 && len != w->w) {
            errors += Report(file, line, 1, "row has %d characters, expected %d (all rows must have equal length)", len, w->w);
        } else if (len > WORLD_MAX_W) {
            errors += Report(file, line, 1, "row is wider than %d cells", WORLD_MAX_W);
        } else {
            if (w->h == 0) w->w = len;
            memcpy(w->grid[w->h], p, (size_t)len);
            w->h++;
        }
        p = *end ? end + 1 : end;
    }
    UnloadFileText(text);
    if (w->name[0] == 0) errors += Report(file, 1, 1, "first line must be the wing name");
    if (w->h < 3 || w->w < 3) errors += Report(file, 2, 1, "grid is missing or too small");
    if (errors) return false;

    /* ---- per-cell checks and object lists ---- */
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            char c = w->grid[z][x];
            bool border = (x == 0 || z == 0 || x == w->w - 1 || z == w->h - 1);
            Vector3 center = { x + 0.5f, 0.0f, z + 0.5f };
            if (!IsKnownChar(c)) { errors += Report(file, z + 2, x + 1, "unknown map character '%c'", c); continue; }
            if (border && !IsWallChar(c) && c != 'E')
                errors += Report(file, z + 2, x + 1, "outer border must be solid (found '%c')", c);
            switch (c) {
            case '@': starts++; w->start = center; break;
            case 'C':
                if (w->chestCount < MAX_CHESTS) w->chests[w->chestCount] = (Cell){ x, z };
                w->chestCount++;
                break;
            case 'E':
                if (w->exitCount < MAX_EXIT_CELLS) w->exits[w->exitCount++] = (Cell){ x, z };
                else errors += Report(file, z + 2, x + 1, "too many exit cells (max %d)", MAX_EXIT_CELLS);
                break;
            case 'O': case 'a':                     /* Sanctum: Master Oren, apprentices/monks */
                if (w->npcCount < MAX_NPCS) {
                    w->npcs[w->npcCount].type = c;
                    w->npcs[w->npcCount].pos = center;
                    w->npcCount++;
                } else errors += Report(file, z + 2, x + 1, "too many NPCs (max %d)", MAX_NPCS);
                break;
            case 's': case 'g': case 'w': case 'Q':
                if (w->spawnCount < MAX_SPAWNS) {
                    w->spawns[w->spawnCount].type = c;
                    w->spawns[w->spawnCount].pos = center;
                    w->spawnCount++;
                } else errors += Report(file, z + 2, x + 1, "too many enemies (max %d)", MAX_SPAWNS);
                break;
            default: break;
            }
        }
    }
    if (starts != 1) errors += Report(file, 2, 1, "map needs exactly one '@' player start (found %d)", starts);
    if (needExit && w->exitCount < 1) errors += Report(file, 2, 1, "map needs at least one 'E' exit door");
    if (w->chestCount != expectedChests)
        errors += Report(file, 2, 1, "map has %d chests 'C', config expects %d", w->chestCount, expectedChests);
    if (w->chestCount > MAX_CHESTS) { w->chestCount = MAX_CHESTS; errors++; }
    if (errors) return false;

    /* ---- flood fill from '@' (chests and the closed exit are solid) ---- */
    memset(reach, 0, sizeof(reach));
    x = (int)w->start.x; z = (int)w->start.z;
    reach[z][x] = 1;
    qx[tail] = (short)x; qz[tail] = (short)z; tail++;
    while (head < tail) {
        int cx = qx[head], cz = qz[head];
        head++;
        for (d = 0; d < 4; d++) {
            int nx = cx + DX[d], nz = cz + DZ[d];
            if (World_IsSolid(w, nx, nz) || reach[nz][nx]) continue;
            reach[nz][nx] = 1;
            qx[tail] = (short)nx; qz[tail] = (short)nz; tail++;
        }
    }
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            char c = w->grid[z][x];
            if (!World_IsSolid(w, x, z) && !reach[z][x]) {
                if (unreachable++ < 8) errors += Report(file, z + 2, x + 1, "floor cell '%c' cannot be reached from '@'", c);
                else errors++;
            }
            if (c == 'C' || c == 'E') {
                bool ok = false;
                for (d = 0; d < 4; d++) {
                    int nx = x + DX[d], nz = z + DZ[d];
                    if (InBounds(w, nx, nz) && reach[nz][nx]) ok = true;
                }
                if (!ok) errors += Report(file, z + 2, x + 1, "%s is not next to reachable floor",
                                          c == 'C' ? "chest 'C'" : "exit 'E'");
            }
        }
    }
    if (errors) return false;

    /* ---- derived data: floor materials, facings ---- */
    for (z = 0; z < w->h; z++)
        for (x = 0; x < w->w; x++)
            if (!IsWallChar(w->grid[z][x]))
                w->floorMat[z][x] = (unsigned char)(IsFloorChar(w->grid[z][x]) ? FloorMat(w->grid[z][x])
                                                                                : NearestFloorMat(w, x, z));
    for (d = 0; d < w->exitCount; d++) {
        Cell e = w->exits[d];
        bool floorAlongX = !World_IsSolid(w, e.x + 1, e.z) || !World_IsSolid(w, e.x - 1, e.z);
        w->exitYaw[d] = floorAlongX ? PI * 0.5f : 0.0f;
    }
    for (d = 0; d < w->chestCount; d++)
        w->chestYaw[d] = OpenYaw(w, w->chests[d].x, w->chests[d].z);
    w->startYaw = OpenYaw(w, (int)w->start.x, (int)w->start.z);
    ClassifyAreas(w);
    for (d = 0; d < w->spawnCount; d++)
        w->spawns[d].yaw = OpenYaw(w, (int)w->spawns[d].pos.x, (int)w->spawns[d].pos.z);
    return true;
}

/* ------------------------------------------------------------ baked light */

/* Is the straight path from a torch to point p free of walls? The last bit near p is skipped,
 * because vertices sit exactly on cell borders (next to their own wall). */
static bool TorchReaches(const World *w, Vector3 from, Vector3 p)
{
    float dx = p.x - from.x, dz = p.z - from.z, len = sqrtf(dx * dx + dz * dz), t;
    for (t = 0.25f; t < len - 0.35f; t += 0.25f) {
        float k = t / len;
        if (World_IsWallCell(w, (int)floorf(from.x + dx * k), (int)floorf(from.z + dz * k))) return false;
    }
    return true;
}

Vector3 World_LightAt(const World *w, Vector3 p)
{
    Vector3 sum = { 0 };
    int i;
    for (i = 0; i < w->torchCount; i++) {
        float d = Vector3Distance(p, w->torches[i].pos), f;
        if (d >= TORCH_RADIUS) continue;
        if (!TorchReaches(w, w->torches[i].pos, p)) continue;
        f = 1.0f - d / TORCH_RADIUS;
        f = f * f * f * TORCH_INTENSITY;      /* cubic falloff: bright pools, dark gaps */
        sum.x += TORCH_COLOR_R * f;
        sum.y += TORCH_COLOR_G * f;
        sum.z += TORCH_COLOR_B * f;
    }
    sum.x = fminf(sum.x, 1.0f);
    sum.y = fminf(sum.y, 1.0f);
    sum.z = fminf(sum.z, 1.0f);
    return sum;
}
