/* architecture.c - builds the 3D world meshes from a wing's grid: one Mesh per
 * (16x16-cell chunk, material). Every face uses world-space UVs (MB_WorldUV), so the repeating
 * material textures flow across cells, and large faces are split into ~1x1 quads (needed for the
 * PS1 affine texturing and vertex wobble). Light from the wall sconces is baked into the vertices. */
#include <string.h>
#include <math.h>
#include "world.h"
#include "meshgen.h"
#include "textures.h"
#include "raymath.h"

static const int DX[4] = { 1, -1, 0, 0 };
static const int DZ[4] = { 0, 0, 1, -1 };

/* ------------------------------------------------------------ geometry builder */

typedef struct {
    MeshBuilder mb[MAT_COUNT];
    const World *w;
} Geo;

static bool IsWallChar(char c) { return c == '#' || c == 'W' || c == 'B' || c == 'P' || c == 'T'; }

static bool InBounds(const World *w, int x, int z) { return x >= 0 && z >= 0 && x < w->w && z < w->h; }

/* Deterministic pseudo-random 0..1 (decorations look the same every run). */
static float Hash01(int x, int y, int seed)
{
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ (unsigned int)seed * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
}

static Color LightAt(const World *w, Vector3 p)
{
    Vector3 l = World_LightAt(w, p);
    return (Color){ (unsigned char)(l.x * 255.0f), (unsigned char)(l.y * 255.0f), (unsigned char)(l.z * 255.0f), 255 };
}

/* A quad a,b,c,d (counter-clockwise from the front) with world-space UVs. */
static void Quad(Geo *g, int mat, Vector3 a, Vector3 b, Vector3 c, Vector3 d)
{
    Vector3 p[4] = { a, b, c, d };
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
    Vector2 uv[4];
    Color l[4];
    int k;
    for (k = 0; k < 4; k++) {
        uv[k] = MB_WorldUV(p[k], n);
        l[k] = LightAt(g->w, Vector3Add(p[k], Vector3Scale(n, 0.05f)));
    }
    MB_Quad(&g->mb[mat], p, uv, l);
}

/* A rectangle from corner o along edges a and b, split into pieces of about 1 unit. */
static void Rect(Geo *g, int mat, Vector3 o, Vector3 a, Vector3 b)
{
    int na = (int)ceilf(Vector3Length(a) - 0.01f), nb = (int)ceilf(Vector3Length(b) - 0.01f), i, j;
    if (na < 1) na = 1;
    if (nb < 1) nb = 1;
    for (j = 0; j < nb; j++) {
        for (i = 0; i < na; i++) {
            Vector3 p0 = Vector3Add(o, Vector3Add(Vector3Scale(a, (float)i / na), Vector3Scale(b, (float)j / nb)));
            Vector3 pa = Vector3Scale(a, 1.0f / na), pb = Vector3Scale(b, 1.0f / nb);
            Quad(g, mat, p0, Vector3Add(p0, pa), Vector3Add(Vector3Add(p0, pa), pb), Vector3Add(p0, pb));
        }
    }
}

/* An axis-aligned box with world-space UVs (all six faces). */
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

/* ------------------------------------------------------------ cells */

static int WallMat(char c)
{
    switch (c) {
    case 'W': return MAT_WOOD_PANEL;
    case 'B': return MAT_BOOKS;
    case 'P': return MAT_PILLAR;
    default:  return MAT_WALL_STONE;
    }
}

/* Wall face of cell (x,z) toward direction d, from the floor to WALL_HEIGHT. */
static void WallFace(Geo *g, int x, int z, int d, int mat)
{
    const float h = WALL_HEIGHT;
    if (d == 0)      Rect(g, mat, (Vector3){ x + 1.0f, 0, z + 1.0f }, (Vector3){ 0, 0, -1 }, (Vector3){ 0, h, 0 });
    else if (d == 1) Rect(g, mat, (Vector3){ (float)x, 0, (float)z }, (Vector3){ 0, 0, 1 }, (Vector3){ 0, h, 0 });
    else if (d == 2) Rect(g, mat, (Vector3){ (float)x, 0, z + 1.0f }, (Vector3){ 1, 0, 0 }, (Vector3){ 0, h, 0 });
    else             Rect(g, mat, (Vector3){ x + 1.0f, 0, (float)z }, (Vector3){ -1, 0, 0 }, (Vector3){ 0, h, 0 });
}

/* Flame position for a torch on wall cell (x,z) facing direction d. */
static Vector3 TorchFlame(int x, int z, int d)
{
    return (Vector3){ x + 0.5f + DX[d] * 0.66f, TORCH_HEIGHT, z + 0.5f + DZ[d] * 0.66f };
}

static void AddTorch(Geo *g, int x, int z, int d)
{
    Vector3 n = { (float)DX[d], 0.0f, (float)DZ[d] }, side = { -n.z, 0.0f, n.x };
    Vector3 wall = { x + 0.5f + n.x * 0.5f, TORCH_HEIGHT, z + 0.5f + n.z * 0.5f };
    Vector3 plate = Vector3Add(wall, Vector3Scale(n, 0.03f)), stick = Vector3Add(wall, Vector3Scale(n, 0.16f));
    float px = fabsf(n.x) * 0.03f + fabsf(side.x) * 0.09f, pz = fabsf(n.z) * 0.03f + fabsf(side.z) * 0.09f;
    Box(g, MAT_METAL, (Vector3){ plate.x - px, TORCH_HEIGHT - 0.46f, plate.z - pz },
                      (Vector3){ plate.x + px, TORCH_HEIGHT - 0.14f, plate.z + pz });                 /* wall plate */
    Box(g, MAT_WOOD, (Vector3){ stick.x - 0.045f, TORCH_HEIGHT - 0.42f, stick.z - 0.045f },
                     (Vector3){ stick.x + 0.045f, TORCH_HEIGHT - 0.08f, stick.z + 0.045f });           /* handle */
}

/* Every face of a 'T' wall that touches open floor carries a torch. Collected before meshing
 * so the baked light can use all of them. */
static void CollectTorches(World *w)
{
    int x, z, d;
    w->torchCount = 0;
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

static void AddBones(Geo *g, int x, int z)
{
    int i;
    for (i = 0; i < 6; i++) {
        float px = x + 0.18f + 0.64f * Hash01(x, z, i * 3 + 1), pz = z + 0.18f + 0.64f * Hash01(x, z, i * 3 + 2);
        float r = Hash01(x, z, i * 3 + 3);
        if (i == 0)          Box(g, MAT_BONE, (Vector3){ px - 0.12f, 0, pz - 0.11f }, (Vector3){ px + 0.12f, 0.22f, pz + 0.11f });
        else if (r < 0.5f)   Box(g, MAT_BONE, (Vector3){ px - 0.2f, 0, pz - 0.035f }, (Vector3){ px + 0.2f, 0.06f, pz + 0.035f });
        else                 Box(g, MAT_BONE, (Vector3){ px - 0.035f, 0, pz - 0.2f }, (Vector3){ px + 0.035f, 0.06f, pz + 0.2f });
    }
}

static void BuildChunk(Geo *g, int cx, int cz)
{
    const World *w = g->w;
    int x, z, d;
    for (z = cz * CHUNK_SIZE; z < (cz + 1) * CHUNK_SIZE && z < w->h; z++) {
        for (x = cx * CHUNK_SIZE; x < (cx + 1) * CHUNK_SIZE && x < w->w; x++) {
            char c = w->grid[z][x];
            if (IsWallChar(c)) {
                for (d = 0; d < 4; d++) {
                    int nx = x + DX[d], nz = z + DZ[d];
                    if (!InBounds(w, nx, nz) || IsWallChar(w->grid[nz][nx])) continue;
                    WallFace(g, x, z, d, WallMat(c));
                    if (c == 'T') AddTorch(g, x, z, d);
                }
            } else {
                Quad(g, w->floorMat[z][x], (Vector3){ (float)x, 0, z + 1.0f }, (Vector3){ x + 1.0f, 0, z + 1.0f },
                     (Vector3){ x + 1.0f, 0, (float)z }, (Vector3){ (float)x, 0, (float)z });
                Quad(g, MAT_CEILING, (Vector3){ (float)x, WALL_HEIGHT, (float)z }, (Vector3){ x + 1.0f, WALL_HEIGHT, (float)z },
                     (Vector3){ x + 1.0f, WALL_HEIGHT, z + 1.0f }, (Vector3){ (float)x, WALL_HEIGHT, z + 1.0f });
                if (c == 'x') AddBones(g, x, z);
            }
        }
    }
}

/* ------------------------------------------------------------ API */

void World_BuildMeshes(World *w)
{
    static Geo g;
    int cx, cz, m;
    int ncx = (w->w + CHUNK_SIZE - 1) / CHUNK_SIZE, ncz = (w->h + CHUNK_SIZE - 1) / CHUNK_SIZE;

    w->partCount = 0;
    w->chunkCount = 0;
    w->vertexCount = 0;
    CollectTorches(w);
    g.w = w;
    for (cz = 0; cz < ncz; cz++) {
        for (cx = 0; cx < ncx; cx++) {
            bool any = false;
            for (m = 0; m < MAT_COUNT; m++) MB_Begin(&g.mb[m]);
            BuildChunk(&g, cx, cz);
            for (m = 0; m < MAT_COUNT; m++) {
                Mesh mesh = MB_End(&g.mb[m]);
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
    }
}

void World_Unload(World *w)
{
    int i;
    for (i = 0; i < w->partCount; i++) UnloadMesh(w->parts[i].mesh);
    w->partCount = 0;
}
