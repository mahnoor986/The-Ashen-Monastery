/* world.c - loads a wing map (text grid), validates it, builds the chunked block meshes,
 * and answers collision / line-of-sight questions. See CLAUDE.md section 5 for the format. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "meshgen.h"
#include "textures.h"
#include "raymath.h"

/* ------------------------------------------------------------ cell types */

static bool IsWallChar(char c)  { return c == '#' || c == 'W' || c == 'B' || c == 'P' || c == 'T'; }
static bool IsFloorChar(char c) { return c == '.' || c == '=' || c == ','; }
static bool IsKnownChar(char c) { return IsWallChar(c) || IsFloorChar(c) || (c && strchr("x@CEsgwQ", c)); }

static int WallTile(char c)
{
    switch (c) {
    case 'W': return TILE_WOOD_WALL;
    case 'B': return TILE_BOOKSHELF;
    case 'P': return TILE_OBSIDIAN;
    default:  return TILE_STONE_WALL;
    }
}

static int FloorTile(char c)
{
    if (c == '=') return TILE_CARPET;
    if (c == ',') return TILE_PLANK_FLOOR;
    return TILE_STONE_FLOOR;
}

/* Deterministic pseudo-random 0..1 (decorations look the same every run). */
static float Hash01(int x, int y, int seed)
{
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ (unsigned int)seed * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
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
static int NearestFloorTile(const World *w, int sx, int sz)
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
        if (IsFloorChar(w->grid[z][x])) return FloorTile(w->grid[z][x]);
        for (d = 0; d < 4; d++) {
            int nx = x + DX[d], nz = z + DZ[d];
            if (!InBounds(w, nx, nz) || seen[nz][nx] || IsWallChar(w->grid[nz][nx])) continue;
            seen[nz][nx] = 1;
            qx[tail] = (short)nx; qz[tail] = (short)nz; tail++;
        }
    }
    return TILE_STONE_FLOOR;
}

bool World_Load(World *w, const char *path, int expectedChests)
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
    if (w->exitCount < 1) errors += Report(file, 2, 1, "map needs at least one 'E' exit door");
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

    /* ---- derived data: floor tiles, facings ---- */
    for (z = 0; z < w->h; z++)
        for (x = 0; x < w->w; x++)
            if (!IsWallChar(w->grid[z][x]))
                w->floorTile[z][x] = (unsigned char)(IsFloorChar(w->grid[z][x]) ? FloorTile(w->grid[z][x])
                                                                                  : NearestFloorTile(w, x, z));
    for (d = 0; d < w->exitCount; d++) {
        Cell e = w->exits[d];
        bool floorAlongX = !World_IsSolid(w, e.x + 1, e.z) || !World_IsSolid(w, e.x - 1, e.z);
        w->exitYaw[d] = floorAlongX ? PI * 0.5f : 0.0f;
    }
    for (d = 0; d < w->chestCount; d++)
        w->chestYaw[d] = OpenYaw(w, w->chests[d].x, w->chests[d].z);
    w->startYaw = OpenYaw(w, (int)w->start.x, (int)w->start.z);
    for (d = 0; d < w->spawnCount; d++)
        w->spawns[d].yaw = OpenYaw(w, (int)w->spawns[d].pos.x, (int)w->spawns[d].pos.z);
    return true;
}

/* ------------------------------------------------------------ meshes */

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

/* Light arriving at a vertex, as a vertex color. */
static Color LightAt(const World *w, Vector3 p)
{
    Vector3 l = World_LightAt(w, p);
    return (Color){ (unsigned char)(l.x * 255.0f), (unsigned char)(l.y * 255.0f), (unsigned char)(l.z * 255.0f), 255 };
}

static void Face(MeshBuilder *mb, const World *w, Vector3 o, Vector3 a, Vector3 b, int tile)
{
    Color l[4];
    l[0] = LightAt(w, o);
    l[1] = LightAt(w, Vector3Add(o, a));
    l[2] = LightAt(w, Vector3Add(Vector3Add(o, a), b));
    l[3] = LightAt(w, Vector3Add(o, b));
    MB_Face(mb, o, a, b, tile, l);
}

/* Box with half extents `along` (in direction n), `side` (across n) and `hy` (vertical). */
static void OrientedBox(MeshBuilder *mb, const World *w, Vector3 c, Vector3 n, float along, float side, float hy, int tile)
{
    float hx = fabsf(n.x) > 0.5f ? along : side;
    float hz = fabsf(n.x) > 0.5f ? side : along;
    MB_Box(mb, (Vector3){ c.x - hx, c.y - hy, c.z - hz }, (Vector3){ c.x + hx, c.y + hy, c.z + hz },
           tile, LightAt(w, c));
}

static void AddTorch(MeshBuilder *mb, const World *w, int x, int z, int d)
{
    Vector3 n = { (float)DX[d], 0.0f, (float)DZ[d] };
    Vector3 wall = { x + 0.5f + n.x * 0.5f, TORCH_HEIGHT, z + 0.5f + n.z * 0.5f };
    Vector3 stick = Vector3Add(wall, Vector3Scale(n, 0.16f));
    stick.y = TORCH_HEIGHT - 0.25f;
    OrientedBox(mb, w, (Vector3){ wall.x + n.x * 0.03f, TORCH_HEIGHT - 0.3f, wall.z + n.z * 0.03f },
                n, 0.03f, 0.09f, 0.16f, TILE_IRON);                          /* wall plate */
    OrientedBox(mb, w, (Vector3){ wall.x + n.x * 0.09f, TORCH_HEIGHT - 0.32f, wall.z + n.z * 0.09f },
                n, 0.07f, 0.03f, 0.03f, TILE_IRON);                          /* arm */
    OrientedBox(mb, w, stick, n, 0.045f, 0.045f, 0.17f, TILE_CEILING);      /* wooden handle */
}

/* Flame position for a torch on wall cell (x,z) facing direction d. */
static Vector3 TorchFlame(int x, int z, int d)
{
    return (Vector3){ x + 0.5f + DX[d] * 0.66f, TORCH_HEIGHT, z + 0.5f + DZ[d] * 0.66f };
}

/* Every face of a 'T' wall that touches open floor carries a torch. Collected before meshing
 * so the baked light can use all of them. */
static void CollectTorches(World *w)
{
    int x, z, d;
    for (z = 0; z < w->h; z++) {
        for (x = 0; x < w->w; x++) {
            if (w->grid[z][x] != 'T') continue;
            for (d = 0; d < 4; d++) {
                int nx = x + DX[d], nz = z + DZ[d];
                if (!InBounds(w, nx, nz) || IsWallChar(w->grid[nz][nx]) || w->torchCount >= MAX_TORCHES) continue;
                w->torches[w->torchCount].pos = TorchFlame(x, z, d);
                w->torches[w->torchCount].normal = (Vector3){ (float)DX[d], 0.0f, (float)DZ[d] };
                w->torchCount++;
            }
        }
    }
}

static void AddBones(MeshBuilder *mb, const World *w, int x, int z)
{
    int i;
    for (i = 0; i < 6; i++) {
        float px = x + 0.18f + 0.64f * Hash01(x, z, i * 3 + 1);
        float pz = z + 0.18f + 0.64f * Hash01(x, z, i * 3 + 2);
        float r = Hash01(x, z, i * 3 + 3);
        Vector3 c = { px, 0.0f, pz };
        if (i == 0) {          /* skull */
            MB_Box(mb, (Vector3){ c.x - 0.12f, 0.0f, c.z - 0.11f }, (Vector3){ c.x + 0.12f, 0.22f, c.z + 0.11f },
                   TILE_BONE, LightAt(w, c));
        } else if (r < 0.5f) { /* long bone along x */
            MB_Box(mb, (Vector3){ c.x - 0.2f, 0.0f, c.z - 0.035f }, (Vector3){ c.x + 0.2f, 0.06f, c.z + 0.035f },
                   TILE_BONE, LightAt(w, c));
        } else {               /* long bone along z */
            MB_Box(mb, (Vector3){ c.x - 0.035f, 0.0f, c.z - 0.2f }, (Vector3){ c.x + 0.035f, 0.06f, c.z + 0.2f },
                   TILE_BONE, LightAt(w, c));
        }
    }
}

static void BuildChunk(MeshBuilder *mb, const World *w, int cx, int cz)
{
    int x, z, d, k;
    for (z = cz * CHUNK_SIZE; z < (cz + 1) * CHUNK_SIZE && z < w->h; z++) {
        for (x = cx * CHUNK_SIZE; x < (cx + 1) * CHUNK_SIZE && x < w->w; x++) {
            char c = w->grid[z][x];
            if (IsWallChar(c)) {
                /* wall faces only where they touch an open cell */
                for (d = 0; d < 4; d++) {
                    int nx = x + DX[d], nz = z + DZ[d];
                    if (!InBounds(w, nx, nz) || IsWallChar(w->grid[nz][nx])) continue;
                    for (k = 0; k < (int)WALL_HEIGHT; k++) {
                        float y = (float)k;
                        int tile = WallTile(c);
                        if (d == 0)      Face(mb, w, (Vector3){ x + 1.0f, y, z + 1.0f }, (Vector3){ 0, 0, -1 }, (Vector3){ 0, 1, 0 }, tile);
                        else if (d == 1) Face(mb, w, (Vector3){ (float)x, y, (float)z }, (Vector3){ 0, 0, 1 }, (Vector3){ 0, 1, 0 }, tile);
                        else if (d == 2) Face(mb, w, (Vector3){ (float)x, y, z + 1.0f }, (Vector3){ 1, 0, 0 }, (Vector3){ 0, 1, 0 }, tile);
                        else             Face(mb, w, (Vector3){ x + 1.0f, y, (float)z }, (Vector3){ -1, 0, 0 }, (Vector3){ 0, 1, 0 }, tile);
                    }
                    if (c == 'T') AddTorch(mb, w, x, z, d);
                }
            } else {
                Face(mb, w, (Vector3){ (float)x, 0.0f, z + 1.0f }, (Vector3){ 1, 0, 0 }, (Vector3){ 0, 0, -1 }, w->floorTile[z][x]);
                Face(mb, w, (Vector3){ (float)x, WALL_HEIGHT, (float)z }, (Vector3){ 1, 0, 0 }, (Vector3){ 0, 0, 1 }, TILE_CEILING);
                if (c == 'x') AddBones(mb, w, x, z);
            }
        }
    }
}

void World_BuildMeshes(World *w)
{
    MeshBuilder mb;
    int cx, cz;
    int ncx = (w->w + CHUNK_SIZE - 1) / CHUNK_SIZE, ncz = (w->h + CHUNK_SIZE - 1) / CHUNK_SIZE;

    w->torchCount = 0;
    w->chunkCount = 0;
    w->vertexCount = 0;
    CollectTorches(w);
    for (cz = 0; cz < ncz; cz++) {
        for (cx = 0; cx < ncx; cx++) {
            Mesh m;
            MB_Begin(&mb);
            BuildChunk(&mb, w, cx, cz);
            m = MB_End(&mb);
            if (m.vertexCount == 0) continue;
            w->vertexCount += m.vertexCount;
            w->chunks[w->chunkCount++] = m;
        }
    }
}

void World_Unload(World *w)
{
    int i;
    for (i = 0; i < w->chunkCount; i++) UnloadMesh(w->chunks[i]);
    w->chunkCount = 0;
}
